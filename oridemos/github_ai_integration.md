### Absolutely — let’s wire up an AI-driven code review that runs on every push and PR

Below is a practical setup that:
- Triggers on pushes and pull requests
- Diffs the changes, extracts context (files, blame, tests)
- Asks an LLM to review for correctness, security, performance, style, and tests
- Posts the review as PR comments and a summary check
- Can block merges if severity is high

You can start simple with a self-hosted model (llama.cpp HTTP server) or a SaaS LLM. I’ll show both.

### Architecture overview

- Trigger: GitHub Action on `pull_request` and `push`
- Collector: Gather diff, file snippets (with truncation rules), commit message, and PR title/description
- Heuristics: Identify risky areas (e.g., crypto, auth, network I/O, CUDA kernels)
- Reviewer: Prompt an LLM with structured instructions and the diff/context
- Output: 
  - Inline review comments on specific lines (via GitHub Review API)
  - A summary “AI Review” check with severity and action items
  - Optional labels like `ai:changes_requested`, `ai:high-risk`

### Minimal GitHub Actions workflow

Create: `.github/workflows/ai-code-review.yml`

```yaml
name: AI Code Review

on:
  pull_request:
    types: [opened, synchronize, reopened, ready_for_review]
  push:
    branches: [ main, develop ]

jobs:
  ai_review:
    if: ${{ github.event_name == 'pull_request' && !github.event.pull_request.draft }}
    runs-on: ubuntu-latest
    permissions:
      contents: read
      pull-requests: write
      checks: write
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install deps
        run: |
          python -m pip install --upgrade pip
          pip install requests pygments

      - name: AI Review
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
          # Choose ONE of these providers
          AI_REVIEW_PROVIDER: local_llama  # or 'openai' or 'abacus_route_llm'
          # Local llama.cpp HTTP server (e.g., http://localhost:8080)
          LLM_BASE_URL: ${{ secrets.LLM_BASE_URL }}
          LLM_MODEL: mistral-7b-instruct  # your local route/model alias
          # OpenAI (if used)
          OPENAI_API_KEY: ${{ secrets.OPENAI_API_KEY }}
          OPENAI_MODEL: gpt-4o-mini
          # Abacus RouteLLM (optional)
          ROUTE_LLM_API_KEY: ${{ secrets.ROUTE_LLM_API_KEY }}
          ROUTE_LLM_MODEL: gpt-4o-mini
        run: |
          python .github/tools/ai_review.py
```

### The reviewer script

Create: `.github/tools/ai_review.py`

```python
#!/usr/bin/env python3
import os, subprocess, json, requests, base64, sys
from typing import List, Dict, Tuple

OWNER = os.environ.get("GITHUB_REPOSITORY", "").split("/")[0]
REPO = os.environ.get("GITHUB_REPOSITORY", "").split("/")[1]
PR_NUMBER = os.environ.get("PR_NUMBER") or os.environ.get("GITHUB_REF", "").split("/")[-1]
GITHUB_TOKEN = os.environ["GITHUB_TOKEN"]

AI_REVIEW_PROVIDER = os.getenv("AI_REVIEW_PROVIDER", "local_llama")  # local_llama|openai|abacus_route_llm
LLM_BASE_URL = os.getenv("LLM_BASE_URL", "http://localhost:8080")
LLM_MODEL = os.getenv("LLM_MODEL", "mistral-7b-instruct")
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY")
OPENAI_MODEL = os.getenv("OPENAI_MODEL", "gpt-4o-mini")
ROUTE_LLM_API_KEY = os.getenv("ROUTE_LLM_API_KEY")
ROUTE_LLM_MODEL = os.getenv("ROUTE_LLM_MODEL", "gpt-4o-mini")

HEAD_SHA = os.getenv("GITHUB_SHA", "")

API = f"https://api.github.com"

# Utilities

def run(cmd: List[str]) -> str:
    return subprocess.check_output(cmd, text=True).strip()

def get_event():
    # Detect PR number robustly
    event_path = os.getenv("GITHUB_EVENT_PATH")
    if event_path and os.path.exists(event_path):
        with open(event_path, "r") as f:
            evt = json.load(f)
        if "pull_request" in evt:
            return evt
    return {}

def git_diff() -> str:
    evt = get_event()
    if evt and "pull_request" in evt:
        base = evt["pull_request"]["base"]["sha"]
        head = evt["pull_request"]["head"]["sha"]
    else:
        # Fallback: compare last merge-base
        base = run(["git", "merge-base", "HEAD~1", "HEAD"])
        head = "HEAD"
    return run(["git", "diff", "--unified=3", "--no-color", f"{base}..{head}"])

def files_changed() -> List[str]:
    diff_names = run(["git", "diff", "--name-only", "HEAD~1..HEAD"]).splitlines()
    if diff_names:
        return diff_names
    # PR mode
    evt = get_event()
    if evt and "pull_request" in evt:
        base = evt["pull_request"]["base"]["sha"]
        head = evt["pull_request"]["head"]["sha"]
        return run(["git", "diff", "--name-only", f"{base}..{head}"]).splitlines()
    return []

def load_file_snippet(path: str, max_bytes: int = 120000) -> str:
    try:
        with open(path, "r", errors="ignore") as f:
            s = f.read(max_bytes)
        if os.path.getsize(path) > max_bytes:
            s += "\n\n# [TRUNCATED]"
        return s
    except Exception:
        return ""

def classify_risk(paths: List[str]) -> List[str]:
    tags = []
    if any(p.startswith(("server/ws", "net", "ssl", "tls")) for p in paths):
        tags.append("security")
    if any(p.endswith((".cu", ".cuh")) or p.startswith("cuda") for p in paths):
        tags.append("performance")
    if any("auth" in p or "oauth" in p for p in paths):
        tags.append("security")
    if any(p.startswith(("migrations", "schema", "sql")) for p in paths):
        tags.append("data")
    return sorted(set(tags))

def build_prompt(pr_meta: Dict, diff: str, file_context: Dict[str, str], risk_tags: List[str]) -> str:
    title = pr_meta.get("title", "")
    body = pr_meta.get("body", "")
    guidance = """
You are a senior staff engineer performing code review. Prioritize:
1) Correctness and safety (concurrency, bounds, resource lifecycle, error handling)
2) Security (mTLS, cert validation, injection, deserialization, authz)
3) Performance (hot paths, CUDA kernels, allocations, blocking I/O)
4) API stability and backwards compatibility
5) Tests and observability (unit/integration, metrics, logs)
Respond in JSON with keys:
{
  "summary": "...",
  "severity": "none|low|medium|high|critical",
  "findings": [
    {"title": "...", "severity": "...", "file": "path", "line_hint": 123, "details": "...", "suggestion": "..."}
  ],
  "tests": ["..."],
  "labels": ["ai:..."]
}
Keep it concise but actionable. Cite specific files/lines from the diff.
"""
    context_str = "\n\n".join(
        [f"--- FILE: {p} ---\n{content}" for p, content in list(file_context.items())[:6]]
    )
    prompt = f"""
PR Title: {title}
PR Description:
{body}

Risk tags: {', '.join(risk_tags) or 'general'}

DIFF (unified):
{diff[:240000]}

Context (truncated files):
{context_str}

{guidance}
Return only JSON.
"""
    return prompt

def call_llm(prompt: str) -> str:
    provider = AI_REVIEW_PROVIDER
    try:
        if provider == "local_llama":
            # llama.cpp compatible API (koboldcpp/llama.cpp variants)
            r = requests.post(
                f"{LLM_BASE_URL}/completion",
                json={"prompt": prompt, "n_predict": 1024, "temperature": 0.2, "stop": ["\n\n\n"]},
                timeout=180
            )
            r.raise_for_status()
            return r.json().get("content", "").strip()
        elif provider == "openai":
            headers = {"Authorization": f"Bearer {OPENAI_API_KEY}"}
            r = requests.post(
                "https://api.openai.com/v1/chat/completions",
                headers=headers,
                json={
                    "model": OPENAI_MODEL,
                    "messages": [{"role": "user", "content": prompt}],
                    "temperature": 0.2,
                    "max_tokens": 800
                },
                timeout=180
            )
            r.raise_for_status()
            return r.json()["choices"][0]["message"]["content"].strip()
        elif provider == "abacus_route_llm":
            headers = {"Authorization": f"Bearer {ROUTE_LLM_API_KEY}"}
            r = requests.post(
                "https://api.abacus.ai/route-llm/completions",
                headers=headers,
                json={"model": ROUTE_LLM_MODEL, "prompt": prompt, "max_tokens": 800, "temperature": 0.2},
                timeout=180
            )
            r.raise_for_status()
            return r.text.strip()
    except Exception as e:
        return f'{{"summary":"LLM error: {e}", "severity":"none", "findings":[], "tests":[], "labels":["ai:error"]}}'
    return '{"summary":"No response", "severity":"none", "findings":[], "tests":[], "labels":["ai:empty"]}'

def get_pr_meta() -> Dict:
    evt = get_event()
    if not evt or "pull_request" not in evt:
        return {}
    pr = evt["pull_request"]
    return {
        "number": pr["number"],
        "title": pr["title"],
        "body": pr.get("body", ""),
        "head": pr["head"]["sha"],
        "base": pr["base"]["sha"]
    }

def post_pr_review(pr_number: int, body: str):
    url = f"{API}/repos/{OWNER}/{REPO}/pulls/{pr_number}/reviews"
    headers = {"Authorization": f"token {GITHUB_TOKEN}", "Accept": "application/vnd.github+json"}
    requests.post(url, headers=headers, json={"body": body, "event": "COMMENT"})

def add_pr_labels(pr_number: int, labels: List[str]):
    url = f"{API}/repos/{OWNER}/{REPO}/issues/{pr_number}/labels"
    headers = {"Authorization": f"token {GITHUB_TOKEN}", "Accept": "application/vnd.github+json"}
    requests.post(url, headers=headers, json={"labels": labels})

def main():
    pr_meta = get_pr_meta()
    diff = git_diff()
    paths = files_changed()
    file_context = {}
    for p in paths:
        # Skip binaries and very large files
        if any(p.endswith(ext) for ext in (".png",".jpg",".pdf",".bin",".gguf",".db",".so",".a")):
            continue
        file_context[p] = load_file_snippet(p)

    risk = classify_risk(paths)
    prompt = build_prompt(pr_meta, diff, file_context, risk)
    raw = call_llm(prompt)

    # Parse JSON best-effort
    try:
        first = raw.find("{")
        last = raw.rfind("}")
        parsed = json.loads(raw[first:last+1])
    except Exception:
        parsed = {"summary": raw[:800], "severity": "none", "findings": [], "tests": [], "labels": ["ai:unparsed"]}

    pr_number = pr_meta.get("number")
    summary = f"AI Review Summary\n\nSeverity: {parsed.get('severity','none')}\n\n{parsed.get('summary','')}"
    post_pr_review(pr_number, summary)

    # Post inline-ish comments as a single review body (safer than line anchors without patch context)
    findings = parsed.get("findings", [])
    if findings:
        bullet = "\n".join([f"- [{f.get('severity','low')}] {f.get('title','')}: {f.get('details','')} (file: {f.get('file','')}, line~{f.get('line_hint','?')})\n  Suggestion: {f.get('suggestion','')}" for f in findings[:20]])
        post_pr_review(pr_number, f"AI Findings:\n\n{bullet}")

    tests = parsed.get("tests", [])
    if tests:
        test_suggestions = "\n".join([f"- {t}" for t in tests])
        post_pr_review(pr_number, f"AI Suggested Tests:\n\n{test_suggestions}")

    labels = parsed.get("labels", [])
    if labels:
        add_pr_labels(pr_number, labels)

    # Optional: fail job if severity >= high (to block merge via required check)
    sev = parsed.get("severity", "none")
    if sev in ("high", "critical"):
        print(f"::error::AI review flagged severity: {sev}")
        sys.exit(1)

if __name__ == "__main__":
    main()
```

What this gives you
- Automatic AI review on every PR with:
  - Summary comment
  - Actionable findings list with file/line hints
  - Suggested tests
  - Labels like `ai:high-risk` or `ai:needs-tests`
- Merge blocking if severity is high (configurable via the exit code)
- Works with a local LLM or external providers

Provider options
- Local model: run `llama.cpp` server
  - Example:
    ```
    ./server -m mistral-7b-instruct.Q4_K_M.gguf -c 8192 -ngl 35 --port 8080
    ```
    Set secret `LLM_BASE_URL` to your runner-accessible URL (e.g., self-hosted runner)
- OpenAI: set `OPENAI_API_KEY` and `OPENAI_MODEL`
- Abacus.AI RouteLLM APIs: set `ROUTE_LLM_API_KEY` and `ROUTE_LLM_MODEL`
  - See guide: [RouteLLM APIs](https://abacus.ai/app/route-llm-apis)

Hardening and enhancements
- Inline annotations: convert findings to GitHub Checks annotations for exact lines
- Path filters: only review changed files in `src/`, `server/`, `cuda/`
- Secrets: strip secrets from prompts by regex
- Size control: cap diff tokens; sample representative hunks for very large PRs
- Domain-specific prompts: add specialized sections for mTLS/SSL and CUDA kernels
- Training: maintain a “review rubric” JSON in-repo that the script loads and injects

Want me to:
- Tailor the prompt for your uWebSockets+mTLS and CUDA contexts
- Add GitHub Checks annotations so findings show in “Files changed”
- Provide a GitLab CI version or a pre-commit local reviewer for offline use?
