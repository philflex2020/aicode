**Excellent use case!** Running regression tests with LLM-assisted analysis and categorization is a powerful workflow. Here's a comprehensive setup for your Jetson + Sparks system:

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│  Test Execution Layer                                   │
│  - Run regression test suites                           │
│  - Collect logs, outputs, metrics                       │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  Data Collection & Storage                              │
│  - SQLite/PostgreSQL for test results                   │
│  - File storage for logs/artifacts                      │
│  - Time-series metrics (optional: InfluxDB/Prometheus)  │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  LLM Analysis Layer                                     │
│  - Categorize failures (crash, timeout, assertion, etc.)│
│  - Extract root causes from logs                        │
│  - Compare with historical patterns                     │
│  - Generate summaries and reports                       │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  Reporting & Visualization                              │
│  - HTML dashboards                                      │
│  - Trend analysis                                       │
│  - Alerts for regressions                               │
└─────────────────────────────────────────────────────────┘
```

---

## 1. Test Execution & Data Collection

### Database Schema for Test Results

```sql
-- test_runs table
CREATE TABLE test_runs (
    run_id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    git_commit TEXT,
    branch TEXT,
    build_config TEXT,
    total_tests INTEGER,
    passed INTEGER,
    failed INTEGER,
    skipped INTEGER,
    duration_seconds REAL
);

-- test_results table
CREATE TABLE test_results (
    result_id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER,
    test_name TEXT,
    test_suite TEXT,
    status TEXT, -- 'pass', 'fail', 'skip', 'timeout', 'crash'
    duration_seconds REAL,
    error_message TEXT,
    log_file TEXT,
    category TEXT, -- LLM-generated category
    root_cause TEXT, -- LLM-extracted root cause
    embedding BLOB, -- vector embedding for similarity search
    FOREIGN KEY (run_id) REFERENCES test_runs(run_id)
);

-- failure_patterns table (for historical analysis)
CREATE TABLE failure_patterns (
    pattern_id INTEGER PRIMARY KEY AUTOINCREMENT,
    category TEXT,
    description TEXT,
    example_error TEXT,
    frequency INTEGER,
    last_seen DATETIME,
    embedding BLOB
);
```

### Test Runner Wrapper Script

```python
#!/usr/bin/env python3
"""
test_runner.py - Executes tests and stores results in DB
"""
import subprocess
import sqlite3
import json
import os
from datetime import datetime
from pathlib import Path

class TestRunner:
    def __init__(self, db_path="test_results.db"):
        self.db_path = db_path
        self.conn = sqlite3.connect(db_path)
        self.init_db()
    
    def init_db(self):
        """Create tables if they don't exist"""
        with open('schema.sql', 'r') as f:
            self.conn.executescript(f.read())
        self.conn.commit()
    
    def run_test_suite(self, test_command, git_commit=None, branch=None):
        """Execute test suite and capture results"""
        start_time = datetime.now()
        
        # Create run record
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO test_runs (git_commit, branch, build_config)
            VALUES (?, ?, ?)
        """, (git_commit, branch, os.environ.get('BUILD_CONFIG', 'default')))
        run_id = cursor.lastrowid
        self.conn.commit()
        
        # Execute tests (example with pytest --json-report)
        result = subprocess.run(
            test_command,
            shell=True,
            capture_output=True,
            text=True
        )
        
        # Parse test results (adjust based on your test framework)
        test_results = self.parse_test_output(result.stdout, result.stderr)
        
        # Store individual test results
        for test in test_results:
            log_file = self.save_log(run_id, test['name'], test.get('output', ''))
            cursor.execute("""
                INSERT INTO test_results 
                (run_id, test_name, test_suite, status, duration_seconds, 
                 error_message, log_file)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            """, (
                run_id,
                test['name'],
                test.get('suite', 'default'),
                test['status'],
                test.get('duration', 0),
                test.get('error', ''),
                log_file
            ))
        
        # Update run summary
        duration = (datetime.now() - start_time).total_seconds()
        stats = self.calculate_stats(test_results)
        cursor.execute("""
            UPDATE test_runs 
            SET total_tests=?, passed=?, failed=?, skipped=?, duration_seconds=?
            WHERE run_id=?
        """, (stats['total'], stats['passed'], stats['failed'], 
              stats['skipped'], duration, run_id))
        
        self.conn.commit()
        return run_id
    
    def parse_test_output(self, stdout, stderr):
        """Parse test framework output - customize for your framework"""
        # Example for pytest JSON output
        # Adjust for your test framework (gtest, ctest, etc.)
        results = []
        # ... parsing logic ...
        return results
    
    def save_log(self, run_id, test_name, output):
        """Save test output to file"""
        log_dir = Path(f"logs/run_{run_id}")
        log_dir.mkdir(parents=True, exist_ok=True)
        log_file = log_dir / f"{test_name.replace('/', '_')}.log"
        log_file.write_text(output)
        return str(log_file)
    
    def calculate_stats(self, results):
        """Calculate pass/fail/skip counts"""
        stats = {'total': len(results), 'passed': 0, 'failed': 0, 'skipped': 0}
        for r in results:
            if r['status'] == 'pass':
                stats['passed'] += 1
            elif r['status'] in ['fail', 'crash', 'timeout']:
                stats['failed'] += 1
            else:
                stats['skipped'] += 1
        return stats

if __name__ == "__main__":
    runner = TestRunner()
    
    # Get git info
    git_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode().strip()
    git_branch = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).decode().strip()
    
    # Run tests
    run_id = runner.run_test_suite(
        "pytest tests/ --json-report --json-report-file=report.json",
        git_commit=git_commit,
        branch=git_branch
    )
    
    print(f"Test run {run_id} completed")
```

---

## 2. LLM Analysis Layer

### Failure Categorization & Root Cause Analysis

```python
#!/usr/bin/env python3
"""
llm_analyzer.py - Analyze test failures using LLM
"""
import sqlite3
import requests
import json
from typing import List, Dict

class LLMAnalyzer:
    def __init__(self, llm_endpoint="http://localhost:8080", db_path="test_results.db"):
        self.llm_endpoint = llm_endpoint
        self.conn = sqlite3.connect(db_path)
    
    def analyze_run(self, run_id: int):
        """Analyze all failures in a test run"""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT result_id, test_name, error_message, log_file
            FROM test_results
            WHERE run_id = ? AND status IN ('fail', 'crash', 'timeout')
        """, (run_id,))
        
        failures = cursor.fetchall()
        
        for result_id, test_name, error_msg, log_file in failures:
            # Read full log
            with open(log_file, 'r') as f:
                log_content = f.read()
            
            # Categorize failure
            category = self.categorize_failure(test_name, error_msg, log_content)
            
            # Extract root cause
            root_cause = self.extract_root_cause(test_name, error_msg, log_content)
            
            # Update database
            cursor.execute("""
                UPDATE test_results
                SET category = ?, root_cause = ?
                WHERE result_id = ?
            """, (category, root_cause, result_id))
        
        self.conn.commit()
        
        # Generate summary report
        return self.generate_summary(run_id)
    
    def categorize_failure(self, test_name: str, error_msg: str, log: str) -> str:
        """Use LLM to categorize the failure type"""
        prompt = f"""You are a test failure analysis expert. Categorize this test failure into ONE of these categories:
- ASSERTION_FAILURE: Test assertion failed
- SEGFAULT: Segmentation fault or memory error
- TIMEOUT: Test exceeded time limit
- NETWORK_ERROR: Network connection or communication failure
- RESOURCE_EXHAUSTION: Out of memory, disk space, etc.
- CONFIGURATION_ERROR: Missing config, wrong environment
- RACE_CONDITION: Timing-dependent failure
- DEPENDENCY_FAILURE: External service or library issue
- REGRESSION: Previously passing test now fails
- FLAKY: Intermittent, non-deterministic failure

Test: {test_name}
Error: {error_msg[:500]}
Log excerpt: {log[-2000:]}

Category (one word only):"""
        
        response = self.llm_complete(prompt, max_tokens=20, temperature=0.1)
        category = response.strip().split()[0].upper()
        
        # Validate category
        valid_categories = [
            'ASSERTION_FAILURE', 'SEGFAULT', 'TIMEOUT', 'NETWORK_ERROR',
            'RESOURCE_EXHAUSTION', 'CONFIGURATION_ERROR', 'RACE_CONDITION',
            'DEPENDENCY_FAILURE', 'REGRESSION', 'FLAKY'
        ]
        
        return category if category in valid_categories else 'UNKNOWN'
    
    def extract_root_cause(self, test_name: str, error_msg: str, log: str) -> str:
        """Use LLM to extract concise root cause"""
        prompt = f"""Analyze this test failure and provide a concise root cause (1-2 sentences max).

Test: {test_name}
Error: {error_msg}
Log excerpt:
{log[-3000:]}

Root cause:"""
        
        return self.llm_complete(prompt, max_tokens=100, temperature=0.3)
    
    def llm_complete(self, prompt: str, max_tokens: int = 512, temperature: float = 0.7) -> str:
        """Send completion request to llama.cpp server"""
        response = requests.post(
            f"{self.llm_endpoint}/completion",
            json={
                "prompt": prompt,
                "n_predict": max_tokens,
                "temperature": temperature,
                "stop": ["\n\n", "---"]
            },
            timeout=60
        )
        
        if response.status_code == 200:
            return response.json()['content'].strip()
        else:
            return "LLM_ERROR"
    
    def generate_summary(self, run_id: int) -> Dict:
        """Generate summary report for the run"""
        cursor = self.conn.cursor()
        
        # Get category breakdown
        cursor.execute("""
            SELECT category, COUNT(*) as count
            FROM test_results
            WHERE run_id = ? AND status != 'pass'
            GROUP BY category
            ORDER BY count DESC
        """, (run_id,))
        
        category_breakdown = dict(cursor.fetchall())
        
        # Get top failures
        cursor.execute("""
            SELECT test_name, category, root_cause
            FROM test_results
            WHERE run_id = ? AND status != 'pass'
            ORDER BY duration_seconds DESC
            LIMIT 10
        """, (run_id,))
        
        top_failures = cursor.fetchall()
        
        # Generate natural language summary using LLM
        summary_prompt = f"""Generate a concise executive summary of this test run:

Total failures: {sum(category_breakdown.values())}
Breakdown by category:
{json.dumps(category_breakdown, indent=2)}

Top failures:
{chr(10).join([f"- {name}: {cause}" for name, cat, cause in top_failures[:5]])}

Summary (3-5 sentences):"""
        
        executive_summary = self.llm_complete(summary_prompt, max_tokens=200, temperature=0.5)
        
        return {
            'run_id': run_id,
            'category_breakdown': category_breakdown,
            'top_failures': top_failures,
            'executive_summary': executive_summary
        }
    
    def find_similar_failures(self, result_id: int, top_k: int = 5) -> List[Dict]:
        """Find historically similar failures using embeddings"""
        # Get embedding for current failure
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT error_message, log_file FROM test_results WHERE result_id = ?
        """, (result_id,))
        
        error_msg, log_file = cursor.fetchone()
        
        with open(log_file, 'r') as f:
            log_content = f.read()
        
        # Get embedding from LLM
        embedding = self.get_embedding(f"{error_msg}\n{log_content[-1000:]}")
        
        # Find similar failures (simplified - use proper vector search in production)
        # In production, use FAISS, ChromaDB, or pgvector for efficient similarity search
        cursor.execute("""
            SELECT result_id, test_name, error_message, category, root_cause
            FROM test_results
            WHERE result_id != ? AND embedding IS NOT NULL
            LIMIT 100
        """, (result_id,))
        
        # Calculate cosine similarity (simplified)
        # ... vector similarity logic ...
        
        return []  # Return top_k similar failures
    
    def get_embedding(self, text: str):
        """Get embedding vector from LLM server"""
        response = requests.post(
            f"{self.llm_endpoint}/embedding",
            json={"content": text},
            timeout=30
        )
        
        if response.status_code == 200:
            return response.json()['embedding']
        return None

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python llm_analyzer.py <run_id>")
        sys.exit(1)
    
    run_id = int(sys.argv[1])
    
    analyzer = LLMAnalyzer()
    summary = analyzer.analyze_run(run_id)
    
    print(json.dumps(summary, indent=2))
```

---

## 3. Reporting & Visualization

### HTML Dashboard Generator

```python
#!/usr/bin/env python3
"""
generate_dashboard.py - Create HTML dashboard for test results
"""
import sqlite3
from datetime import datetime, timedelta
from pathlib import Path

def generate_dashboard(db_path="test_results.db", output_path="dashboard.html"):
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    # Get recent runs
    cursor.execute("""
        SELECT run_id, timestamp, git_commit, branch, 
               total_tests, passed, failed, duration_seconds
        FROM test_runs
        ORDER BY timestamp DESC
        LIMIT 20
    """)
    recent_runs = cursor.fetchall()
    
    # Get failure trends
    cursor.execute("""
        SELECT category, COUNT(*) as count
        FROM test_results
        WHERE status != 'pass' 
          AND run_id IN (SELECT run_id FROM test_runs ORDER BY timestamp DESC LIMIT 10)
        GROUP BY category
        ORDER BY count DESC
    """)
    failure_categories = cursor.fetchall()
    
    # Generate HTML
    html = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Test Results Dashboard</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        table {{ border-collapse: collapse; width: 100%; margin: 20px 0; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #4CAF50; color: white; }}
        .pass {{ color: green; }}
        .fail {{ color: red; }}
        .chart {{ margin: 20px 0; }}
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <h1>Regression Test Dashboard</h1>
    <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
    
    <h2>Recent Test Runs</h2>
    <table>
        <tr>
            <th>Run ID</th>
            <th>Timestamp</th>
            <th>Branch</th>
            <th>Commit</th>
            <th>Total</th>
            <th>Passed</th>
            <th>Failed</th>
            <th>Duration</th>
        </tr>
"""
    
    for run in recent_runs:
        run_id, ts, commit, branch, total, passed, failed, duration = run
        pass_rate = (passed / total * 100) if total > 0 else 0
        color_class = 'pass' if failed == 0 else 'fail'
        
        html += f"""
        <tr class="{color_class}">
            <td><a href="run_{run_id}.html">{run_id}</a></td>
            <td>{ts}</td>
            <td>{branch}</td>
            <td>{commit[:8]}</td>
            <td>{total}</td>
            <td>{passed} ({pass_rate:.1f}%)</td>
            <td>{failed}</td>
            <td>{duration:.1f}s</td>
        </tr>
"""
    
    html += """
    </table>
    
    <h2>Failure Categories (Last 10 Runs)</h2>
    <canvas id="categoryChart" width="400" height="200"></canvas>
    <script>
        const ctx = document.getElementById('categoryChart').getContext('2d');
        new Chart(ctx, {
            type: 'bar',
            data: {
                labels: """ + str([cat[0] for cat in failure_categories]) + """,
                datasets: [{
                    label: 'Failure Count',
                    data: """ + str([cat[1] for cat in failure_categories]) + """,
                    backgroundColor: 'rgba(255, 99, 132, 0.2)',
                    borderColor: 'rgba(255, 99, 132, 1)',
                    borderWidth: 1
                }]
            },
            options: {
                scales: {
                    y: { beginAtZero: true }
                }
            }
        });
    </script>
</body>
</html>
"""
    
    Path(output_path).write_text(html)
    print(f"Dashboard generated: {output_path}")

if __name__ == "__main__":
    generate_dashboard()
```

---

## 4. Integration & Automation

### CI/CD Integration Script

```bash
#!/bin/bash
# ci_test_pipeline.sh - Full test + analysis pipeline

set -e

# 1. Build project
echo "Building project..."
cmake --build build -j

# 2. Run tests
echo "Running regression tests..."
python3 test_runner.py

# Get the run_id from the last run
RUN_ID=$(sqlite3 test_results.db "SELECT MAX(run_id) FROM test_runs")

# 3. Analyze failures with LLM
echo "Analyzing failures with LLM..."
python3 llm_analyzer.py $RUN_ID > analysis_${RUN_ID}.json

# 4. Generate dashboard
echo "Generating dashboard..."
python3 generate_dashboard.py

# 5. Check for regressions
FAILED=$(sqlite3 test_results.db "SELECT failed FROM test_runs WHERE run_id=$RUN_ID")

if [ "$FAILED" -gt 0 ]; then
    echo "⚠️  $FAILED test(s) failed in run $RUN_ID"
    echo "Analysis summary:"
    cat analysis_${RUN_ID}.json | jq '.executive_summary'
    exit 1
else
    echo "✅ All tests passed in run $RUN_ID"
    exit 0
fi
```

---

## 5. Advanced Features

### Flaky Test Detection

```python
def detect_flaky_tests(db_path="test_results.db", window_days=7):
    """Identify tests that pass/fail intermittently"""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    cutoff = datetime.now() - timedelta(days=window_days)
    
    cursor.execute("""
        SELECT test_name, 
               SUM(CASE WHEN status = 'pass' THEN 1 ELSE 0 END) as passes,
               SUM(CASE WHEN status != 'pass' THEN 1 ELSE 0 END) as failures,
               COUNT(*) as total
        FROM test_results tr
        JOIN test_runs run ON tr.run_id = run.run_id
        WHERE run.timestamp > ?
        GROUP BY test_name
        HAVING passes > 0 AND failures > 0 AND total >= 5
        ORDER BY (failures * 1.0 / total) DESC
    """, (cutoff,))
    
    flaky_tests = cursor.fetchall()
    
    return [
        {
            'test': name,
            'passes': passes,
            'failures': failures,
            'flake_rate': failures / total
        }
        for name, passes, failures, total in flaky_tests
    ]
```

### Regression Detection (Comparing Runs)

```python
def detect_regressions(current_run_id, baseline_run_id, db_path="test_results.db"):
    """Find tests that passed in baseline but fail now"""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    cursor.execute("""
        SELECT 
            curr.test_name,
            curr.status as current_status,
            base.status as baseline_status,
            curr.root_cause
        FROM test_results curr
        JOIN test_results base ON curr.test_name = base.test_name
        WHERE curr.run_id = ?
          AND base.run_id = ?
          AND base.status = 'pass'
          AND curr.status != 'pass'
    """, (current_run_id, baseline_run_id))
    
    regressions = cursor.fetchall()
    
    return [
        {
            'test': name,
            'current_status': curr_status,
            'baseline_status': base_status,
            'root_cause': cause
        }
        for name, curr_status, base_status, cause in regressions
    ]
```

---

## 6. Recommended Model for Test Analysis

When your Sparks arrive, use:

**Best choice: Llama 3.1 8B Instruct** or **Mixtral 8x7B Instruct**
- Good at structured analysis and categorization
- Fast enough for real-time analysis
- Strong reasoning for root cause extraction

**For embeddings/similarity search:**
- Run a separate embedding model (e.g., `nomic-embed-text` or use Llama's embedding mode)
- Store in vector DB for fast similarity search

---

## 7. Deployment Architecture with Sparks

```
┌──────────────┐
│ Jetson Orin  │  - Test execution
│              │  - Data collection
│              │  - Embedding generation
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ NVIDIA Sparks│  - Primary LLM inference (Mixtral 8x7B / CodeLlama 34B)
│              │  - Failure analysis
│              │  - Report generation
└──────────────┘
```

Start LLM server on Sparks:
```bash
./build/bin/llama-server \
  -m ~/models/mixtral-8x7b-instruct.Q4_K_M.gguf \
  --host 0.0.0.0 --port 8080 \
  -ngl 999 \
  --ctx-size 16384 \
  -t $(nproc)
```

---

This gives you a complete system for:
✅ Automated test execution
✅ Structured result storage
✅ LLM-powered failure categorization
✅ Root cause extraction
✅ Historical pattern matching
✅ Flaky test detection
✅ Regression identification
✅ Automated reporting

Let me know which test framework you're using (pytest, gtest, ctest, etc.) and I can customize the parsers!
