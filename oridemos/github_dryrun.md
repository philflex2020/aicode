### Great idea — let’s add a Dry-Run mode

I’ll extend the GitHub integration with a `dry_run` flag that:
- Prints exactly what would be created (projects, milestones, issues, comments).
- Skips all API calls.
- Still builds maps and simulated IDs so dependencies/comments resolve in output.

Below is a drop-in replacement for `github_integration.py` with dry-run support.

```python
#!/usr/bin/env python3
"""
github_integration.py - Export project plan to GitHub Issues and Projects

Features:
- Dry-run mode: preview everything without making API calls
- Classic Projects creation (REST v3)
- Milestones, issues, dependency comments

Env:
- GITHUB_TOKEN (required unless dry_run=True and you don't call live methods)
"""

import sqlite3
import json
import requests
from typing import Dict, List, Optional
import os
from datetime import datetime

class GitHubIntegration:
    def __init__(self, token: Optional[str], owner: str, repo: str,
                 db_path: str = "project_planning.db", dry_run: bool = False):
        """
        Args:
            token: GitHub PAT (repo + project). Can be None in dry_run mode.
            owner: Repository owner (username or org)
            repo: Repository name
            dry_run: If True, do not make any API calls; only print planned actions
        """
        self.token = token
        self.owner = owner
        self.repo = repo
        self.base_url = "https://api.github.com"
        self.dry_run = dry_run
        self.headers = {
            "Authorization": f"token {token}" if token else "",
            "Accept": "application/vnd.github.v3+json"
        }
        self.conn = sqlite3.connect(db_path)

        if not dry_run and not token:
            raise ValueError("GITHUB_TOKEN is required when dry_run is False")

        # Internal counters for simulated IDs in dry-run
        self._sim_issue = 1000
        self._sim_project = 500
        self._sim_milestone = 50

    def export_to_github(self, narrative_id: int, create_project: bool = True,
                         create_milestones: bool = True) -> Dict:
        print(f"📤 Exporting project {narrative_id} to GitHub {self.owner}/{self.repo}...")
        if self.dry_run:
            print("🧪 DRY-RUN: No API calls will be made. Showing planned actions only.\n")

        results = {
            'project_id': None,
            'milestones': [],
            'issues': [],
            'errors': []
        }

        cursor = self.conn.cursor()
        cursor.execute(
            "SELECT title, description FROM product_narratives WHERE narrative_id = ?",
            (narrative_id,)
        )
        row = cursor.fetchone()
        if not row:
            raise ValueError(f"narrative_id {narrative_id} not found")
        project_title, project_desc = row

        # Create Classic Project
        project_id = None
        if create_project:
            project_id = self.create_project(project_title, project_desc)
            results['project_id'] = project_id
            print(f"  ✅ Project (classic): {project_id}")

        # Milestones
        milestone_map = {}
        if create_milestones:
            cursor.execute("""
                SELECT milestone_id, name, description, target_date
                FROM project_milestones
                WHERE narrative_id = ?
                ORDER BY milestone_id
            """, (narrative_id,))
            for ms_id, name, desc, target_date in cursor.fetchall():
                milestone_number = self.create_milestone(name, desc, target_date)
                if milestone_number:
                    milestone_map[ms_id] = milestone_number
                    results['milestones'].append(milestone_number)
                    print(f"  ✅ Milestone: {name} -> #{milestone_number}")
                else:
                    results['errors'].append(f"Failed to create milestone: {name}")

        # Components -> issues
        cursor.execute("""
            SELECT component_id, name, description, status, complexity_estimate
            FROM components WHERE narrative_id = ?
        """, (narrative_id,))
        component_issue_map: Dict[int, int] = {}
        for comp_id, name, desc, status, complexity in cursor.fetchall():
            labels = ['component']
            if status: labels.append(str(status))
            if complexity: labels.append(str(complexity))
            title = f"Component: {name}"
            body = desc or ""
            number = self.create_issue(title=title, body=body, labels=labels)
            if number:
                component_issue_map[comp_id] = number
                results['issues'].append(number)
                print(f"  ✅ Issue #{number}: {title} [labels={labels}]")
            else:
                results['errors'].append(f"Failed to create component issue: {name}")

        # Tasks -> issues + dependency comments
        cursor.execute("""
            SELECT task_id, component_id, title, description, task_type,
                   priority, estimated_hours, dependencies, milestone_id
            FROM development_tasks
            WHERE narrative_id = ?
            ORDER BY priority, task_id
        """, (narrative_id,))
        task_issue_map: Dict[int, int] = {}
        pending_dependency_comments: List[Dict] = []

        for (task_id, comp_id, title, desc, task_type, priority,
             hours, deps_json, ms_id) in cursor.fetchall():
            labels = [task_type or 'task', f'priority-{priority}']
            body = desc or ""
            if comp_id and comp_id in component_issue_map:
                body = f"Related to component #{component_issue_map[comp_id]}\n\n" + body
            if hours:
                body += f"\n\nEstimated effort: {hours:.1f} hours"

            milestone_number = milestone_map.get(ms_id) if ms_id else None

            number = self.create_issue(
                title=title, body=body, labels=labels, milestone=milestone_number
            )
            if number:
                task_issue_map[task_id] = number
                results['issues'].append(number)
                ms_info = f", milestone=#{milestone_number}" if milestone_number else ""
                print(f"  ✅ Issue #{number}: {title} [labels={labels}{ms_info}]")

                dependencies = json.loads(deps_json) if deps_json else []
                if dependencies:
                    pending_dependency_comments.append({
                        'issue_number': number,
                        'deps': dependencies
                    })
            else:
                results['errors'].append(f"Failed to create task issue: {title}")

        # Dependency comments
        for item in pending_dependency_comments:
            refs = [f"#{task_issue_map[d]}" for d in item['deps'] if d in task_issue_map]
            if refs:
                self.add_comment(item['issue_number'], f"Dependencies: {', '.join(refs)}")

        # Gaps -> issues
        cursor.execute("""
            SELECT g.gap_id, g.component_id, g.gap_type, g.description, g.impact, g.suggested_solution
            FROM component_gaps g
            JOIN components c ON g.component_id = c.component_id
            WHERE c.narrative_id = ?
        """, (narrative_id,))
        for gap_id, comp_id, gap_type, gap_desc, impact, solution in cursor.fetchall():
            labels = ['gap']
            if gap_type: labels.append(str(gap_type))
            if impact: labels.append(str(impact))
            body = gap_desc or ""
            if solution:
                body += f"\n\nSuggested solution:\n{solution}"
            if comp_id and comp_id in component_issue_map:
                body = f"Related to component #{component_issue_map[comp_id]}\n\n" + body

            title = f"Gap: {gap_desc[:80] if gap_desc else 'Unspecified'}"
            number = self.create_issue(title=title, body=body, labels=labels)
            if number:
                results['issues'].append(number)
                print(f"  ✅ Gap issue #{number}: {title} [labels={labels}]")
            else:
                results['errors'].append("Failed to create a gap issue")

        print("\n✅ Export complete!")
        print(f"   Project ID: {results['project_id']}")
        print(f"   Milestones: {len(results['milestones'])}")
        print(f"   Issues: {len(results['issues'])}")
        if results['errors']:
            print(f"   Errors: {len(results['errors'])}")

        return results

    # ------------ Helper methods ------------

    def _sim_next_issue(self) -> int:
        self._sim_issue += 1
        return self._sim_issue

    def _sim_next_project(self) -> int:
        self._sim_project += 1
        return self._sim_project

    def _sim_next_milestone(self) -> int:
        self._sim_milestone += 1
        return self._sim_milestone

    def create_project(self, name: str, description: str) -> Optional[int]:
        if self.dry_run:
            pid = self._sim_next_project()
            print(f"[DRY-RUN] Would create Classic Project: name='{name}' body='{(description or '')[:80]}...' -> id={pid}")
            return pid

        payload = {"name": name, "body": description or ""}
        try:
            r = requests.post(
                f"{self.base_url}/repos/{self.owner}/{self.repo}/projects",
                json=payload,
                headers={**self.headers, "Accept": "application/vnd.github.inertia-preview+json"},
                timeout=30
            )
            if r.status_code == 201:
                return r.json().get("id")
            else:
                print(f"Error creating project: {r.status_code} - {r.text}")
                return None
        except Exception as e:
            print(f"Exception creating project: {e}")
            return None

    def create_milestone(self, title: str, description: str, due_on: Optional[str]) -> Optional[int]:
        if self.dry_run:
            mid = self._sim_next_milestone()
            print(f"[DRY-RUN] Would create Milestone: title='{title}', due='{due_on}' -> number={mid}")
            return mid

        payload = {
            "title": title,
            "state": "open",
            "description": description or ""
        }
        if due_on:
            try:
                if len(due_on) == 10:
                    dt = datetime.strptime(due_on, "%Y-%m-%d")
                    payload["due_on"] = dt.isoformat() + "Z"
                else:
                    payload["due_on"] = due_on
            except Exception:
                pass

        try:
            r = requests.post(
                f"{self.base_url}/repos/{self.owner}/{self.repo}/milestones",
                json=payload,
                headers=self.headers,
                timeout=30
            )
            if r.status_code == 201:
                return r.json().get("number")
            else:
                print(f"Error creating milestone: {r.status_code} - {r.text}")
                return None
        except Exception as e:
            print(f"Exception creating milestone: {e}")
            return None

    def create_issue(self, title: str, body: str, labels: List[str] = None,
                     milestone: Optional[int] = None, assignees: List[str] = None) -> Optional[int]:
        if self.dry_run:
            num = self._sim_next_issue()
            print(f"[DRY-RUN] Would create Issue: title='{title}', labels={labels or []}, milestone={milestone}")
            return num

        payload = {
            "title": title,
            "body": body or "",
            "labels": labels or []
        }
        if milestone:
            payload["milestone"] = milestone
        if assignees:
            payload["assignees"] = assignees

        try:
            r = requests.post(
                f"{self.base_url}/repos/{self.owner}/{self.repo}/issues",
                json=payload,
                headers=self.headers,
                timeout=30
            )
            if r.status_code == 201:
                return r.json().get("number")
            else:
                print(f"Error creating issue: {r.status_code} - {r.text}")
                return None
        except Exception as e:
            print(f"Exception creating issue: {e}")
            return None

    def add_comment(self, issue_number: int, body: str) -> bool:
        if self.dry_run:
            print(f"[DRY-RUN] Would add comment to issue #{issue_number}: '{body}'")
            return True

        try:
            r = requests.post(
                f"{self.base_url}/repos/{self.owner}/{self.repo}/issues/{issue_number}/comments",
                json={"body": body},
                headers=self.headers,
                timeout=30
            )
            if r.status_code == 201:
                return True
            else:
                print(f"Error adding comment: {r.status_code} - {r.text}")
                return False
        except Exception as e:
            print(f"Exception adding comment: {e}")
            return False


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 4:
        print("Usage: python github_integration.py <narrative_id> <owner> <repo> [--dry-run]")
        print("\nEnvironment variables:")
        print("  GITHUB_TOKEN - PAT with repo + project scopes (not required in --dry-run)")
        sys.exit(1)

    narrative_id = int(sys.argv[1])
    owner = sys.argv[2]
    repo = sys.argv[3]
    dry_run = ("--dry-run" in sys.argv)

    token = os.getenv("GITHUB_TOKEN")
    if dry_run:
        print("Running in DRY-RUN mode; GITHUB_TOKEN is optional.")
    elif not token:
        print("❌ Missing GITHUB_TOKEN (required without --dry-run)")
        sys.exit(1)

    gh = GitHubIntegration(token, owner, repo, dry_run=dry_run)
    gh.export_to_github(narrative_id, create_project=True, create_milestones=True)

    print("\n✅ GitHub export finished (dry-run=" + str(dry_run) + ")")
```

#### How to run (dry-run)
- No token needed:
```
python github_integration.py <narrative_id> <owner> <repo> --dry-run
```

You’ll see a full preview of:
- Project to be created
- Milestones (with due dates)
- Issues for components, tasks, and gaps
- Dependency comments referencing simulated issue numbers

Want me to also add a dry-run mode to Jira export and to the Risk/Gantt modules’ file outputs (e.g., “simulate write”)?

