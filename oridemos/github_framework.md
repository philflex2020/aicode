You're right—I cut off the GitHub integration mid-file. Here’s the complete, working GitHub integration module with issue creation, milestones, project (classic) creation, comments, and basic dependency notes. I’ve also included clear notes on Projects v2 if you prefer that.

### github_integration.py (complete)

```python
#!/usr/bin/env python3
"""
github_integration.py - Export project plan to GitHub Issues and Projects

Notes:
- This uses GitHub REST v3.
- Projects v2 (beta/GraphQL) is different; this module creates a Classic Project for simplicity.
- Provide a PAT with repo + project scope.
"""

import sqlite3
import json
import requests
from typing import Dict, List, Optional
import os
from datetime import datetime

class GitHubIntegration:
    def __init__(self, token: str, owner: str, repo: str, db_path: str = "project_planning.db"):
        """
        Initialize GitHub integration

        Args:
            token: GitHub personal access token (needs: repo, project)
            owner: Repository owner (username or org)
            repo: Repository name
        """
        self.token = token
        self.owner = owner
        self.repo = repo
        self.base_url = "https://api.github.com"
        self.headers = {
            "Authorization": f"token {token}",
            "Accept": "application/vnd.github.v3+json"
        }
        self.conn = sqlite3.connect(db_path)

    def export_to_github(self, narrative_id: int, create_project: bool = True,
                         create_milestones: bool = True) -> Dict:
        """
        Export project plan to GitHub

        Args:
            narrative_id: Project ID
            create_project: Create GitHub Project (Classic)
            create_milestones: Create GitHub milestones

        Returns:
            Dictionary with created entity identifiers
        """
        print(f"📤 Exporting project {narrative_id} to GitHub {self.owner}/{self.repo}...")

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

        # Create GitHub Project (Classic) if enabled
        project_id = None
        if create_project:
            project_id = self.create_project(project_title, project_desc)
            if project_id:
                results['project_id'] = project_id
                print(f"  ✅ Created project (classic): {project_id}")
            else:
                print("  ⚠️ Failed to create project (classic)")

        # Create milestones if enabled
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
                    print(f"  ✅ Created milestone: {name}")
                else:
                    results['errors'].append(f"Failed to create milestone: {name}")

        # Create issues for components
        cursor.execute("""
            SELECT component_id, name, description, status, complexity_estimate
            FROM components WHERE narrative_id = ?
        """, (narrative_id,))
        component_issue_map: Dict[int, int] = {}

        for comp_id, name, desc, status, complexity in cursor.fetchall():
            labels = ['component']
            if status: labels.append(str(status))
            if complexity: labels.append(str(complexity))
            number = self.create_issue(
                title=f"Component: {name}",
                body=desc or "",
                labels=labels
            )
            if number:
                component_issue_map[comp_id] = number
                results['issues'].append(number)
                print(f"  ✅ Created issue #{number}: Component {name}")
            else:
                results['errors'].append(f"Failed to create component issue: {name}")

        # Create issues for development tasks
        cursor.execute("""
            SELECT task_id, component_id, title, description, task_type,
                   priority, estimated_hours, dependencies, milestone_id
            FROM development_tasks
            WHERE narrative_id = ?
            ORDER BY priority, task_id
        """, (narrative_id,))
        task_issue_map: Dict[int, int] = {}
        pending_dependency_comments: List[Dict] = []

        for task_id, comp_id, title, desc, task_type, priority, hours, deps_json, ms_id in cursor.fetchall():
            labels = [task_type or 'task', f'priority-{priority}']
            body = desc or ""
            if comp_id and comp_id in component_issue_map:
                body = f"Related to component #{component_issue_map[comp_id]}\n\n" + body
            if hours:
                body += f"\n\nEstimated effort: {hours:.1f} hours"

            milestone_number = None
            if ms_id and ms_id in milestone_map:
                milestone_number = milestone_map[ms_id]

            issue_number = self.create_issue(
                title=title,
                body=body,
                labels=labels,
                milestone=milestone_number
            )
            if issue_number:
                task_issue_map[task_id] = issue_number
                results['issues'].append(issue_number)
                print(f"  ✅ Created issue #{issue_number}: {title}")

                # Capture dependency comments to post after mapping is more populated
                dependencies = json.loads(deps_json) if deps_json else []
                if dependencies:
                    pending_dependency_comments.append({
                        'issue_number': issue_number,
                        'deps': dependencies
                    })
            else:
                results['errors'].append(f"Failed to create task issue: {title}")

        # Post dependency comments (best effort, only those already created)
        for item in pending_dependency_comments:
            refs = [f"#{task_issue_map[d]}" for d in item['deps'] if d in task_issue_map]
            if refs:
                self.add_comment(item['issue_number'],
                                 f"Dependencies: {', '.join(refs)}")

        # Create issues for gaps
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

            issue_number = self.create_issue(
                title=f"Gap: {gap_desc[:80] if gap_desc else 'Unspecified'}",
                body=body,
                labels=labels
            )
            if issue_number:
                results['issues'].append(issue_number)
                print(f"  ✅ Created gap issue #{issue_number}")
            else:
                results['errors'].append("Failed to create a gap issue")

        print("\n✅ Export complete!")
        print(f"   Project ID: {results['project_id']}")
        print(f"   Milestones: {len(results['milestones'])}")
        print(f"   Issues: {len(results['issues'])}")
        if results['errors']:
            print(f"   Errors: {len(results['errors'])}")

        return results

    # --- REST helpers ---

    def create_project(self, name: str, description: str) -> Optional[int]:
        """
        Create a Classic Project on the repo.
        Returns project id or None.
        """
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
        """
        Create a milestone. due_on should be ISO 8601 (YYYY-MM-DD or full timestamp).
        Returns milestone number or None.
        """
        payload = {
            "title": title,
            "state": "open",
            "description": description or ""
        }
        # Try to coerce date if it looks like a date
        if due_on:
            try:
                # Accept YYYY-MM-DD
                if len(due_on) == 10:
                    dt = datetime.strptime(due_on, "%Y-%m-%d")
                    payload["due_on"] = dt.isoformat() + "Z"
                else:
                    # Otherwise assume already ISO8601
                    payload["due_on"] = due_on
            except Exception:
                # Ignore malformed date
                pass

        try:
            r = requests.post(
                f"{self.base_url}/repos/{self.owner}/{self.repo}/milestones",
                json=payload,
                headers=self.headers,
                timeout=30
            )
            if r.status_code in (201,):
                return r.json().get("number")
            else:
                print(f"Error creating milestone: {r.status_code} - {r.text}")
                return None
        except Exception as e:
            print(f"Exception creating milestone: {e}")
            return None

    def create_issue(self, title: str, body: str, labels: List[str] = None,
                     milestone: Optional[int] = None, assignees: List[str] = None) -> Optional[int]:
        """
        Create an issue. Returns issue number or None.
        """
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
            if r.status_code in (201,):
                return r.json().get("number")
            else:
                print(f"Error creating issue: {r.status_code} - {r.text}")
                return None
        except Exception as e:
            print(f"Exception creating issue: {e}")
            return None

    def add_comment(self, issue_number: int, body: str) -> bool:
        """
        Add a comment to an issue. Returns True on success.
        """
        try:
            r = requests.post(
                f"{self.base_url}/repos/{self.owner}/{self.repo}/issues/{issue_number}/comments",
                json={"body": body},
                headers=self.headers,
                timeout=30
            )
            if r.status_code in (201,):
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
        print("Usage: python github_integration.py <narrative_id> <owner> <repo>")
        print("\nEnvironment variables required:")
        print("  GITHUB_TOKEN - Personal Access Token with repo + project scope")
        sys.exit(1)

    narrative_id = int(sys.argv[1])
    owner = sys.argv[2]
    repo = sys.argv[3]

    token = os.getenv("GITHUB_TOKEN")
    if not token:
        print("❌ Missing GITHUB_TOKEN")
        sys.exit(1)

    gh = GitHubIntegration(token, owner, repo)
    results = gh.export_to_github(narrative_id, create_project=True, create_milestones=True)

    print("\n✅ Exported to GitHub")
```

### How to run

1) Set env:
- GITHUB_TOKEN: PAT with repo and project scopes

2) Execute:
```
python github_integration.py <narrative_id> <owner> <repo>
```

Example:
```
python github_integration.py 1 my-org my-repo
```

### Notes and options
- Classic Projects vs Projects v2:
  - The above `create_project` uses Classic Projects API. For Projects v2 (GraphQL), let me know and I’ll provide a separate minimal GraphQL helper to create a Project, fields, and add issues to it.
- Milestones date format:
  - Accepts YYYY-MM-DD; internally converted to ISO8601.

If you want, I can also add:
- Automatic assignment based on `assigned_to` from your DB.
- Labels based on component types or risk tags.
- A dry-run mode that prints what would be created without hitting the API.
