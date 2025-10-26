**Perfect!** Let's build out the complete enterprise-grade project management system with Jira/GitHub integration, Gantt charts, and risk analysis.

---

## Extended System Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Product Narrative Analysis (existing)                  │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  Risk Analysis Engine                                   │
│  - Technical risks                                      │
│  - Schedule risks                                       │
│  - Resource risks                                       │
│  - Mitigation strategies                                │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  Project Management Integration                         │
│  - Jira issue creation                                  │
│  - GitHub issues/projects                               │
│  - Gantt chart generation                               │
│  - Resource allocation                                  │
└─────────────────────────────────────────────────────────┘
```

---

## 1. Risk Analysis Module

```python
#!/usr/bin/env python3
"""
risk_analyzer.py - Identify and analyze project risks
"""
import sqlite3
import json
import requests
from typing import List, Dict
from datetime import datetime

class RiskAnalyzer:
    def __init__(self, llm_endpoint="http://localhost:8080", db_path="project_planning.db"):
        self.llm_endpoint = llm_endpoint
        self.conn = sqlite3.connect(db_path)
        self.init_risk_tables()
    
    def init_risk_tables(self):
        """Create risk analysis tables"""
        self.conn.executescript("""
            CREATE TABLE IF NOT EXISTS project_risks (
                risk_id INTEGER PRIMARY KEY AUTOINCREMENT,
                narrative_id INTEGER,
                risk_category TEXT, -- technical, schedule, resource, external, quality
                risk_type TEXT, -- specific type within category
                description TEXT,
                probability TEXT, -- very_low, low, medium, high, very_high
                impact TEXT, -- negligible, minor, moderate, major, critical
                risk_score REAL, -- probability * impact (0-25)
                affected_components TEXT, -- JSON array of component_ids
                affected_tasks TEXT, -- JSON array of task_ids
                indicators TEXT, -- early warning signs
                mitigation_strategy TEXT,
                contingency_plan TEXT,
                owner TEXT,
                status TEXT DEFAULT 'identified', -- identified, mitigating, resolved, occurred
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (narrative_id) REFERENCES product_narratives(narrative_id)
            );
            
            CREATE TABLE IF NOT EXISTS risk_dependencies (
                dependency_id INTEGER PRIMARY KEY AUTOINCREMENT,
                risk_id INTEGER,
                depends_on_risk_id INTEGER,
                dependency_type TEXT, -- causes, amplifies, mitigates
                FOREIGN KEY (risk_id) REFERENCES project_risks(risk_id),
                FOREIGN KEY (depends_on_risk_id) REFERENCES project_risks(risk_id)
            );
            
            CREATE TABLE IF NOT EXISTS risk_events (
                event_id INTEGER PRIMARY KEY AUTOINCREMENT,
                risk_id INTEGER,
                event_type TEXT, -- status_change, probability_change, impact_change, occurred
                description TEXT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (risk_id) REFERENCES project_risks(risk_id)
            );
        """)
        self.conn.commit()
    
    def analyze_project_risks(self, narrative_id: int) -> List[Dict]:
        """Comprehensive risk analysis for a project"""
        print("🎲 Analyzing project risks...")
        
        # Get project context
        context = self.get_project_context(narrative_id)
        
        # Identify different risk categories
        technical_risks = self.identify_technical_risks(narrative_id, context)
        schedule_risks = self.identify_schedule_risks(narrative_id, context)
        resource_risks = self.identify_resource_risks(narrative_id, context)
        external_risks = self.identify_external_risks(narrative_id, context)
        quality_risks = self.identify_quality_risks(narrative_id, context)
        
        all_risks = (technical_risks + schedule_risks + resource_risks + 
                     external_risks + quality_risks)
        
        # Store risks
        for risk in all_risks:
            self.store_risk(narrative_id, risk)
        
        # Analyze risk dependencies
        self.analyze_risk_dependencies(narrative_id)
        
        # Generate risk matrix
        risk_matrix = self.generate_risk_matrix(narrative_id)
        
        print(f"✅ Identified {len(all_risks)} risks")
        return all_risks
    
    def get_project_context(self, narrative_id: int) -> Dict:
        """Get project context for risk analysis"""
        cursor = self.conn.cursor()
        
        # Get narrative
        cursor.execute("""
            SELECT title, description FROM product_narratives WHERE narrative_id = ?
        """, (narrative_id,))
        title, description = cursor.fetchone()
        
        # Get components
        cursor.execute("""
            SELECT name, status, complexity_estimate, effort_estimate_hours
            FROM components WHERE narrative_id = ?
        """, (narrative_id,))
        components = cursor.fetchall()
        
        # Get tasks
        cursor.execute("""
            SELECT COUNT(*), SUM(estimated_hours)
            FROM development_tasks WHERE narrative_id = ?
        """, (narrative_id,))
        task_count, total_effort = cursor.fetchone()
        
        # Get gaps
        cursor.execute("""
            SELECT COUNT(*) FROM component_gaps g
            JOIN components c ON g.component_id = c.component_id
            WHERE c.narrative_id = ? AND g.impact IN ('blocker', 'critical')
        """, (narrative_id,))
        critical_gaps = cursor.fetchone()[0]
        
        return {
            'title': title,
            'description': description,
            'components': components,
            'task_count': task_count,
            'total_effort': total_effort or 0,
            'critical_gaps': critical_gaps,
            'missing_components': len([c for c in components if c[1] == 'missing']),
            'complex_components': len([c for c in components if c[2] in ['complex', 'very_complex']])
        }
    
    def identify_technical_risks(self, narrative_id: int, context: Dict) -> List[Dict]:
        """Identify technical risks"""
        cursor = self.conn.cursor()
        
        # Get components with high complexity
        cursor.execute("""
            SELECT component_id, name, description, complexity_estimate, dependencies
            FROM components 
            WHERE narrative_id = ? 
              AND complexity_estimate IN ('complex', 'very_complex')
        """, (narrative_id,))
        
        complex_components = cursor.fetchall()
        
        risks = []
        
        # Analyze each complex component
        for comp_id, name, desc, complexity, deps_json in complex_components:
            prompt = f"""Analyze technical risks for this component:

Component: {name}
Description: {desc}
Complexity: {complexity}
Dependencies: {deps_json}

Identify specific technical risks such as:
- Technology maturity issues
- Integration challenges
- Performance bottlenecks
- Scalability concerns
- Security vulnerabilities
- Technical debt

For each risk, provide:
- Risk type (specific technical issue)
- Description (what could go wrong)
- Probability (very_low, low, medium, high, very_high)
- Impact (negligible, minor, moderate, major, critical)
- Early warning indicators
- Mitigation strategy
- Contingency plan

Format as JSON array:
[{{
  "type": "...",
  "description": "...",
  "probability": "...",
  "impact": "...",
  "indicators": "...",
  "mitigation": "...",
  "contingency": "..."
}}]

Risks JSON:"""
            
            response = self.llm_complete(prompt, max_tokens=2048, temperature=0.4)
            
            try:
                json_start = response.find('[')
                json_end = response.rfind(']') + 1
                if json_start >= 0 and json_end > json_start:
                    component_risks = json.loads(response[json_start:json_end])
                    
                    for risk in component_risks:
                        risk['category'] = 'technical'
                        risk['affected_components'] = [comp_id]
                        risks.append(risk)
            except:
                pass
        
        # General technical risks
        general_tech_risks = self.identify_general_technical_risks(context)
        risks.extend(general_tech_risks)
        
        return risks
    
    def identify_general_technical_risks(self, context: Dict) -> List[Dict]:
        """Identify general technical risks based on project characteristics"""
        risks = []
        
        # High number of missing components
        if context['missing_components'] > 5:
            risks.append({
                'category': 'technical',
                'type': 'architecture_complexity',
                'description': f"High number of missing components ({context['missing_components']}) increases integration risk",
                'probability': 'high',
                'impact': 'major',
                'indicators': 'Integration tests failing, interface mismatches',
                'mitigation': 'Early integration testing, clear interface contracts, regular architecture reviews',
                'contingency': 'Simplify architecture, reduce component count, use proven patterns',
                'affected_components': []
            })
        
        # Large project size
        if context['total_effort'] > 1000:
            risks.append({
                'category': 'technical',
                'type': 'project_scale',
                'description': f"Large project size ({context['total_effort']:.0f} hours) increases coordination complexity",
                'probability': 'medium',
                'impact': 'major',
                'indicators': 'Communication breakdowns, duplicate work, integration issues',
                'mitigation': 'Modular architecture, clear ownership, automated testing, CI/CD',
                'contingency': 'Break into smaller phases, reduce scope, add coordination resources',
                'affected_components': []
            })
        
        # Critical gaps
        if context['critical_gaps'] > 0:
            risks.append({
                'category': 'technical',
                'type': 'critical_dependencies',
                'description': f"{context['critical_gaps']} critical gaps identified that block progress",
                'probability': 'high',
                'impact': 'critical',
                'indicators': 'Tasks blocked, dependencies not resolved',
                'mitigation': 'Prioritize gap resolution, parallel development where possible',
                'contingency': 'Use temporary workarounds, mock implementations, reduce scope',
                'affected_components': []
            })
        
        return risks
    
    def identify_schedule_risks(self, narrative_id: int, context: Dict) -> List[Dict]:
        """Identify schedule-related risks"""
        cursor = self.conn.cursor()
        
        risks = []
        
        # Get task dependencies
        cursor.execute("""
            SELECT task_id, title, estimated_hours, dependencies
            FROM development_tasks
            WHERE narrative_id = ? AND dependencies IS NOT NULL AND dependencies != '[]'
        """, (narrative_id,))
        
        dependent_tasks = cursor.fetchall()
        
        # Analyze critical path
        if len(dependent_tasks) > 10:
            risks.append({
                'category': 'schedule',
                'type': 'critical_path_complexity',
                'description': f"Complex task dependencies ({len(dependent_tasks)} dependent tasks) create schedule risk",
                'probability': 'medium',
                'impact': 'major',
                'indicators': 'Tasks waiting on dependencies, cascading delays',
                'mitigation': 'Identify critical path, add buffer time, parallel work streams',
                'contingency': 'Fast-track critical tasks, add resources, reduce scope',
                'affected_components': [],
                'affected_tasks': [t[0] for t in dependent_tasks]
            })
        
        # Estimation uncertainty
        if context['complex_components'] > 3:
            risks.append({
                'category': 'schedule',
                'type': 'estimation_uncertainty',
                'description': f"High complexity components ({context['complex_components']}) have uncertain estimates",
                'probability': 'high',
                'impact': 'moderate',
                'indicators': 'Tasks taking longer than estimated, frequent re-estimation',
                'mitigation': 'Add 30-50% buffer for complex tasks, spike solutions, iterative estimation',
                'contingency': 'Re-baseline schedule, reduce scope, add resources',
                'affected_components': []
            })
        
        # Aggressive timeline
        weeks_estimate = context['total_effort'] / 30  # Assuming 6h/day, 5 days/week
        if weeks_estimate > 26:  # More than 6 months
            risks.append({
                'category': 'schedule',
                'type': 'long_duration',
                'description': f"Long project duration (~{weeks_estimate:.0f} weeks) increases schedule risk",
                'probability': 'medium',
                'impact': 'moderate',
                'indicators': 'Scope creep, team changes, priority shifts',
                'mitigation': 'Phase delivery, regular milestones, scope management',
                'contingency': 'Break into smaller releases, MVP approach',
                'affected_components': []
            })
        
        return risks
    
    def identify_resource_risks(self, narrative_id: int, context: Dict) -> List[Dict]:
        """Identify resource-related risks"""
        risks = []
        
        # Skill requirements
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT DISTINCT component_type FROM components WHERE narrative_id = ?
        """, (narrative_id,))
        
        component_types = [row[0] for row in cursor.fetchall()]
        
        if len(component_types) > 5:
            risks.append({
                'category': 'resource',
                'type': 'skill_diversity',
                'description': f"Project requires diverse skills ({len(component_types)} component types)",
                'probability': 'medium',
                'impact': 'moderate',
                'indicators': 'Difficulty finding qualified developers, knowledge silos',
                'mitigation': 'Cross-training, pair programming, documentation, external expertise',
                'contingency': 'Simplify architecture, use familiar technologies, hire specialists',
                'affected_components': []
            })
        
        # Team size requirements
        optimal_team_size = max(2, int(context['total_effort'] / 480))  # 480h = 3 months per person
        
        if optimal_team_size > 5:
            risks.append({
                'category': 'resource',
                'type': 'team_scaling',
                'description': f"Project requires large team (~{optimal_team_size} developers)",
                'probability': 'medium',
                'impact': 'major',
                'indicators': 'Communication overhead, coordination issues, onboarding delays',
                'mitigation': 'Clear ownership, modular architecture, communication protocols',
                'contingency': 'Extend timeline, reduce scope, improve tooling',
                'affected_components': []
            })
        
        # Key person dependency
        risks.append({
            'category': 'resource',
            'type': 'key_person_dependency',
            'description': 'Risk of key personnel leaving or becoming unavailable',
            'probability': 'low',
            'impact': 'major',
            'indicators': 'Knowledge concentrated in few people, lack of documentation',
            'mitigation': 'Knowledge sharing, documentation, pair programming, cross-training',
            'contingency': 'Hire replacements, extend timeline, reduce scope',
            'affected_components': []
        })
        
        return risks
    
    def identify_external_risks(self, narrative_id: int, context: Dict) -> List[Dict]:
        """Identify external risks"""
        risks = []
        
        # Third-party dependencies
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT name, dependencies FROM components 
            WHERE narrative_id = ? AND dependencies IS NOT NULL AND dependencies != '[]'
        """, (narrative_id,))
        
        components_with_deps = cursor.fetchall()
        
        if components_with_deps:
            risks.append({
                'category': 'external',
                'type': 'third_party_dependencies',
                'description': 'Dependency on external libraries, APIs, or services',
                'probability': 'medium',
                'impact': 'moderate',
                'indicators': 'API changes, library deprecations, service outages',
                'mitigation': 'Version pinning, abstraction layers, fallback options, monitoring',
                'contingency': 'Alternative libraries, in-house implementation, graceful degradation',
                'affected_components': []
            })
        
        # Requirement changes
        risks.append({
            'category': 'external',
            'type': 'requirement_changes',
            'description': 'Stakeholder requirements may change during development',
            'probability': 'medium',
            'impact': 'moderate',
            'indicators': 'Frequent requirement clarifications, stakeholder disagreements',
            'mitigation': 'Clear requirements documentation, change control process, regular reviews',
            'contingency': 'Agile approach, MVP focus, modular design for flexibility',
            'affected_components': []
        })
        
        # Compliance/regulatory
        if 'compliance' in context['description'].lower() or 'regulation' in context['description'].lower():
            risks.append({
                'category': 'external',
                'type': 'compliance_requirements',
                'description': 'Compliance or regulatory requirements may impact design',
                'probability': 'medium',
                'impact': 'major',
                'indicators': 'Audit findings, regulatory changes, certification delays',
                'mitigation': 'Early compliance review, expert consultation, regular audits',
                'contingency': 'Compliance-first redesign, certification support, timeline extension',
                'affected_components': []
            })
        
        return risks
    
    def identify_quality_risks(self, narrative_id: int, context: Dict) -> List[Dict]:
        """Identify quality-related risks"""
        risks = []
        
        # Test coverage
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT COUNT(*) FROM test_plans WHERE narrative_id = ?
        """, (narrative_id,))
        
        test_plan_count = cursor.fetchone()[0]
        
        if test_plan_count < 3:
            risks.append({
                'category': 'quality',
                'type': 'insufficient_testing',
                'description': 'Limited test coverage may lead to quality issues',
                'probability': 'medium',
                'impact': 'major',
                'indicators': 'Bugs in production, customer complaints, rework',
                'mitigation': 'Comprehensive test plan, automated testing, code reviews, QA resources',
                'contingency': 'Extended testing phase, beta program, phased rollout',
                'affected_components': []
            })
        
        # Technical debt
        cursor.execute("""
            SELECT COUNT(*) FROM component_gaps
            WHERE gap_type = 'tech_debt'
        """, ())
        
        tech_debt_count = cursor.fetchone()[0]
        
        if tech_debt_count > 0:
            risks.append({
                'category': 'quality',
                'type': 'technical_debt',
                'description': f'Existing technical debt ({tech_debt_count} items) may impact quality',
                'probability': 'medium',
                'impact': 'moderate',
                'indicators': 'Increasing bug rate, slow feature development, refactoring needs',
                'mitigation': 'Allocate time for debt reduction, refactoring sprints, code quality standards',
                'contingency': 'Major refactoring, architecture redesign, extended timeline',
                'affected_components': []
            })
        
        # Performance requirements
        if 'performance' in context['description'].lower() or 'latency' in context['description'].lower():
            risks.append({
                'category': 'quality',
                'type': 'performance_requirements',
                'description': 'Strict performance requirements may be difficult to meet',
                'probability': 'medium',
                'impact': 'major',
                'indicators': 'Performance tests failing, optimization needed, architecture changes',
                'mitigation': 'Early performance testing, profiling, optimization, load testing',
                'contingency': 'Architecture changes, caching, horizontal scaling, relaxed requirements',
                'affected_components': []
            })
        
        return risks
    
    def store_risk(self, narrative_id: int, risk: Dict):
        """Store risk in database"""
        cursor = self.conn.cursor()
        
        # Calculate risk score
        prob_values = {'very_low': 1, 'low': 2, 'medium': 3, 'high': 4, 'very_high': 5}
        impact_values = {'negligible': 1, 'minor': 2, 'moderate': 3, 'major': 4, 'critical': 5}
        
        prob = prob_values.get(risk.get('probability', 'medium'), 3)
        impact = impact_values.get(risk.get('impact', 'moderate'), 3)
        risk_score = prob * impact
        
        cursor.execute("""
            INSERT INTO project_risks
            (narrative_id, risk_category, risk_type, description, probability, impact,
             risk_score, affected_components, affected_tasks, indicators,
             mitigation_strategy, contingency_plan)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            narrative_id,
            risk.get('category', 'technical'),
            risk.get('type', 'unknown'),
            risk.get('description', ''),
            risk.get('probability', 'medium'),
            risk.get('impact', 'moderate'),
            risk_score,
            json.dumps(risk.get('affected_components', [])),
            json.dumps(risk.get('affected_tasks', [])),
            risk.get('indicators', ''),
            risk.get('mitigation', ''),
            risk.get('contingency', '')
        ))
        
        self.conn.commit()
    
    def analyze_risk_dependencies(self, narrative_id: int):
        """Analyze how risks relate to each other"""
        cursor = self.conn.cursor()
        
        # Get all risks for this project
        cursor.execute("""
            SELECT risk_id, risk_type, description FROM project_risks
            WHERE narrative_id = ?
        """, (narrative_id,))
        
        risks = cursor.fetchall()
        
        # Use LLM to identify dependencies
        if len(risks) > 1:
            risk_list = "\n".join([f"{r[0]}: {r[1]} - {r[2]}" for r in risks])
            
            prompt = f"""Analyze dependencies between these project risks:

{risk_list}

Identify which risks:
- CAUSE other risks (if this happens, it triggers another risk)
- AMPLIFY other risks (if this happens, another risk becomes worse)
- MITIGATE other risks (if this is addressed, another risk is reduced)

Format as JSON array:
[{{"risk_id": 1, "depends_on": 2, "type": "causes|amplifies|mitigates"}}]

Dependencies JSON:"""
            
            response = self.llm_complete(prompt, max_tokens=512, temperature=0.3)
            
            try:
                json_start = response.find('[')
                json_end = response.rfind(']') + 1
                if json_start >= 0 and json_end > json_start:
                    dependencies = json.loads(response[json_start:json_end])
                    
                    for dep in dependencies:
                        cursor.execute("""
                            INSERT INTO risk_dependencies
                            (risk_id, depends_on_risk_id, dependency_type)
                            VALUES (?, ?, ?)
                        """, (dep['risk_id'], dep['depends_on'], dep['type']))
                    
                    self.conn.commit()
            except:
                pass
    
    def generate_risk_matrix(self, narrative_id: int) -> Dict:
        """Generate risk matrix (probability vs impact)"""
        cursor = self.conn.cursor()
        
        cursor.execute("""
            SELECT risk_id, risk_type, description, probability, impact, risk_score
            FROM project_risks
            WHERE narrative_id = ?
            ORDER BY risk_score DESC
        """, (narrative_id,))
        
        risks = cursor.fetchall()
        
        matrix = {
            'very_high_prob': {'critical': [], 'major': [], 'moderate': [], 'minor': [], 'negligible': []},
            'high_prob': {'critical': [], 'major': [], 'moderate': [], 'minor': [], 'negligible': []},
            'medium_prob': {'critical': [], 'major': [], 'moderate': [], 'minor': [], 'negligible': []},
            'low_prob': {'critical': [], 'major': [], 'moderate': [], 'minor': [], 'negligible': []},
            'very_low_prob': {'critical': [], 'major': [], 'moderate': [], 'minor': [], 'negligible': []}
        }
        
        for risk in risks:
            risk_id, risk_type, desc, prob, impact, score = risk
            prob_key = f"{prob}_prob"
            if prob_key in matrix and impact in matrix[prob_key]:
                matrix[prob_key][impact].append({
                    'id': risk_id,
                    'type': risk_type,
                    'description': desc,
                    'score': score
                })
        
        return matrix
    
    def llm_complete(self, prompt: str, max_tokens: int = 512, temperature: float = 0.7) -> str:
        """Send completion request to LLM"""
        try:
            response = requests.post(
                f"{self.llm_endpoint}/completion",
                json={
                    "prompt": prompt,
                    "n_predict": max_tokens,
                    "temperature": temperature,
                    "stop": ["\n\n\n"]
                },
                timeout=120
            )
            
            if response.status_code == 200:
                return response.json()['content'].strip()
        except Exception as e:
            print(f"LLM error: {e}")
        
        return ""

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python risk_analyzer.py <narrative_id>")
        sys.exit(1)
    
    narrative_id = int(sys.argv[1])
    
    analyzer = RiskAnalyzer()
    risks = analyzer.analyze_project_risks(narrative_id)
    
    print(f"\n✅ Risk analysis complete!")
    print(f"Total risks identified: {len(risks)}")
    
    # Print top risks
    cursor = analyzer.conn.cursor()
    cursor.execute("""
        SELECT risk_type, description, probability, impact, risk_score
        FROM project_risks
        WHERE narrative_id = ?
        ORDER BY risk_score DESC
        LIMIT 5
    """, (narrative_id,))
    
    print("\n🔴 Top 5 Risks:")
    for i, (rtype, desc, prob, impact, score) in enumerate(cursor.fetchall(), 1):
        print(f"{i}. [{score:.0f}] {rtype}: {desc[:80]}...")
```

---

## 2. Gantt Chart Generator

```python
#!/usr/bin/env python3
"""
gantt_generator.py - Generate Gantt charts for project timeline
"""
import sqlite3
import json
from datetime import datetime, timedelta
from pathlib import Path

class GanttGenerator:
    def __init__(self, db_path="project_planning.db"):
        self.conn = sqlite3.connect(db_path)
    
    def generate_gantt(self, narrative_id: int, start_date: datetime = None,
                       team_size: int = 3, hours_per_day: int = 6,
                       output_format: str = 'html') -> str:
        """
        Generate Gantt chart
        
        Args:
            narrative_id: Project ID
            start_date: Project start date (default: today)
            team_size: Number of developers
            hours_per_day: Productive hours per developer per day
            output_format: 'html', 'mermaid', or 'json'
        """
        if start_date is None:
            start_date = datetime.now()
        
        # Get tasks with dependencies
        tasks = self.get_tasks_with_schedule(narrative_id, start_date, team_size, hours_per_day)
        
        # Generate output based on format
        if output_format == 'html':
            return self.generate_html_gantt(narrative_id, tasks, start_date)
        elif output_format == 'mermaid':
            return self.generate_mermaid_gantt(tasks)
        elif output_format == 'json':
            return json.dumps(tasks, indent=2, default=str)
        else:
            raise ValueError(f"Unknown format: {output_format}")
    
    def get_tasks_with_schedule(self, narrative_id: int, start_date: datetime,
                                 team_size: int, hours_per_day: int) -> list:
        """Calculate task schedule considering dependencies and resources"""
        cursor = self.conn.cursor()
        
        # Get all tasks
        cursor.execute("""
            SELECT task_id, title, description, task_type, priority,
                   estimated_hours, dependencies, component_id
            FROM development_tasks
            WHERE narrative_id = ?
            ORDER BY priority, task_id
        """, (narrative_id,))
        
        tasks = []
        for row in cursor.fetchall():
            task_id, title, desc, task_type, priority, hours, deps_json, comp_id = row
            
            dependencies = json.loads(deps_json) if deps_json else []
            
            tasks.append({
                'id': task_id,
                'title': title,
                'description': desc,
                'type': task_type,
                'priority': priority,
                'estimated_hours': hours,
                'dependencies': dependencies,
                'component_id': comp_id,
                'start_date': None,
                'end_date': None,
                'assigned_to': None
            })
        
        # Calculate schedule using critical path method
        tasks = self.calculate_schedule(tasks, start_date, team_size, hours_per_day)
        
        return tasks
    
    def calculate_schedule(self, tasks: list, start_date: datetime,
                           team_size: int, hours_per_day: int) -> list:
        """Calculate task schedule using resource leveling"""
        # Build dependency graph
        task_map = {t['id']: t for t in tasks}
        
        # Calculate earliest start dates (forward pass)
        for task in tasks:
            if not task['dependencies']:
                # No dependencies, can start immediately
                task['earliest_start'] = start_date
            else:
                # Start after all dependencies complete
                dep_end_dates = []
                for dep_id in task['dependencies']:
                    if dep_id in task_map and task_map[dep_id].get('earliest_end'):
                        dep_end_dates.append(task_map[dep_id]['earliest_end'])
                
                if dep_end_dates:
                    task['earliest_start'] = max(dep_end_dates)
                else:
                    task['earliest_start'] = start_date
            
            # Calculate duration in working days
            duration_days = task['estimated_hours'] / hours_per_day
            task['earliest_end'] = self.add_working_days(task['earliest_start'], duration_days)
        
        # Resource leveling (simplified - assign to available team member)
        team_availability = [start_date] * team_size  # Track when each team member is free
        
        for task in sorted(tasks, key=lambda t: (t['earliest_start'], t['priority'])):
            # Find first available team member after earliest start
            earliest_available_idx = 0
            earliest_available_date = team_availability[0]
            
            for i, avail_date in enumerate(team_availability):
                if avail_date <= task['earliest_start']:
                    earliest_available_idx = i
                    earliest_available_date = task['earliest_start']
                    break
                elif avail_date < earliest_available_date:
                    earliest_available_idx = i
                    earliest_available_date = avail_date
            
            # Assign task
            task['start_date'] = max(task['earliest_start'], earliest_available_date)
            duration_days = task['estimated_hours'] / hours_per_day
            task['end_date'] = self.add_working_days(task['start_date'], duration_days)
            task['assigned_to'] = f"Dev {earliest_available_idx + 1}"
            
            # Update team member availability
            team_availability[earliest_available_idx] = task['end_date']
        
        return tasks
    
    def add_working_days(self, start_date: datetime, days: float) -> datetime:
        """Add working days (skip weekends)"""
        current = start_date
        days_added = 0
        
        while days_added < days:
            current += timedelta(days=1)
            # Skip weekends
            if current.weekday() < 5:  # Monday = 0, Friday = 4
                days_added += 1
        
        return current
    
    def generate_html_gantt(self, narrative_id: int, tasks: list, start_date: datetime) -> str:
        """Generate interactive HTML Gantt chart"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT title FROM product_narratives WHERE narrative_id = ?", (narrative_id,))
        project_title = cursor.fetchone()[0]
        
        # Calculate project duration
        if tasks:
            project_end = max(t['end_date'] for t in tasks)
            total_days = (project_end - start_date).days
        else:
            project_end = start_date
            total_days = 0
        
        # Generate HTML with embedded JavaScript for interactivity
        html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Gantt Chart: {project_title}</title>
    <script src="https://cdn.jsdelivr.net/npm/frappe-gantt@0.6.1/dist/frappe-gantt.min.js"></script>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/frappe-gantt@0.6.1/dist/frappe-gantt.css">
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        h1 {{ color: #2c3e50; }}
        .gantt-container {{ margin: 20px 0; }}
        .summary {{ background-color: #ecf0f1; padding: 15px; border-radius: 5px; margin: 20px 0; }}
        .legend {{ margin: 20px 0; }}
        .legend-item {{ display: inline-block; margin-right: 20px; }}
        .legend-color {{ display: inline-block; width: 20px; height: 20px; margin-right: 5px; vertical-align: middle; }}
    </style>
</head>
<body>
    <h1>Project Timeline: {project_title}</h1>
    
    <div class="summary">
        <strong>Project Duration:</strong> {total_days} days<br/>
        <strong>Start Date:</strong> {start_date.strftime('%Y-%m-%d')}<br/>
        <strong>End Date:</strong> {project_end.strftime('%Y-%m-%d')}<br/>
        <strong>Total Tasks:</strong> {len(tasks)}
    </div>
    
    <div class="legend">
        <div class="legend-item">
            <span class="legend-color" style="background-color: #3498db;"></span>
            <span>Design</span>
        </div>
        <div class="legend-item">
            <span class="legend-color" style="background-color: #2ecc71;"></span>
            <span>Implementation</span>
        </div>
        <div class="legend-item">
            <span class="legend-color" style="background-color: #f39c12;"></span>
            <span>Testing</span>
        </div>
        <div class="legend-item">
            <span class="legend-color" style="background-color: #9b59b6;"></span>
            <span>Documentation</span>
        </div>
    </div>
    
    <div class="gantt-container">
        <svg id="gantt"></svg>
    </div>
    
    <script>
        const tasks = {json.dumps([{
            'id': str(t['id']),
            'name': t['title'],
            'start': t['start_date'].strftime('%Y-%m-%d'),
            'end': t['end_date'].strftime('%Y-%m-%d'),
            'progress': 0,
            'dependencies': ','.join([str(d) for d in t['dependencies']]) if t['dependencies'] else '',
            'custom_class': t['type']
        } for t in tasks], indent=2)};
        
        const gantt = new Gantt("#gantt", tasks, {{
            view_mode: 'Day',
            date_format: 'YYYY-MM-DD',
            custom_popup_html: function(task) {{
                return `
                    <div class="details-container">
                        <h5>${{task.name}}</h5>
                        <p>Start: ${{task._start.toLocaleDateString()}}</p>
                        <p>End: ${{task._end.toLocaleDateString()}}</p>
                        <p>Duration: ${{task.duration}} days</p>
                    </div>
                `;
            }}
        }});
        
        // View mode selector
        const viewModes = ['Quarter Day', 'Half Day', 'Day', 'Week', 'Month'];
        let currentMode = 2;
        
        document.addEventListener('keydown', function(e) {{
            if (e.key === 'ArrowLeft') {{
                currentMode = Math.max(0, currentMode - 1);
                gantt.change_view_mode(viewModes[currentMode]);
            }} else if (e.key === 'ArrowRight') {{
                currentMode = Math.min(viewModes.length - 1, currentMode + 1);
                gantt.change_view_mode(viewModes[currentMode]);
            }}
        }});
    </script>
    
    <p><small>Use arrow keys to change view mode</small></p>
</body>
</html>
"""
        
        # Save to file
        output_path = Path("reports") / f"gantt_{narrative_id}.html"
        output_path.parent.mkdir(exist_ok=True)
        output_path.write_text(html)
        
        print(f"✅ Gantt chart generated: {output_path}")
        return str(output_path)
    
    def generate_mermaid_gantt(self, tasks: list) -> str:
        """Generate Mermaid Gantt chart (for markdown/documentation)"""
        mermaid = "```mermaid\ngantt\n"
        mermaid += "    title Project Timeline\n"
        mermaid += "    dateFormat YYYY-MM-DD\n"
        mermaid += "    section Tasks\n"
        
        for task in tasks:
            # Mermaid format: task_name :task_id, start_date, duration
            duration_days = (task['end_date'] - task['start_date']).days
            mermaid += f"    {task['title']} :task{task['id']}, {task['start_date'].strftime('%Y-%m-%d')}, {duration_days}d\n"
        
        mermaid += "```\n"
        
        return mermaid

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python gantt_generator.py <narrative_id> [team_size] [hours_per_day]")
        sys.exit(1)
    
    narrative_id = int(sys.argv[1])
    team_size = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    hours_per_day = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    
    generator = GanttGenerator()
    gantt_path = generator.generate_gantt(
        narrative_id,
        team_size=team_size,
        hours_per_day=hours_per_day,
        output_format='html'
    )
    
    print(f"\n✅ Gantt chart ready: {gantt_path}")
```

---

## 3. Jira Integration

```python
#!/usr/bin/env python3
"""
jira_integration.py - Export project plan to Jira
"""
import sqlite3
import json
import requests
from typing import Dict, List
import os

class JiraIntegration:
    def __init__(self, jira_url: str, email: str, api_token: str, db_path="project_planning.db"):
        """
        Initialize Jira integration
        
        Args:
            jira_url: Jira instance URL (e.g., https://yourcompany.atlassian.net)
            email: Your Jira email
            api_token: Jira API token (generate at https://id.atlassian.com/manage/api-tokens)
        """
        self.jira_url = jira_url.rstrip('/')
        self.auth = (email, api_token)
        self.conn = sqlite3.connect(db_path)
        self.headers = {
            "Accept": "application/json",
            "Content-Type": "application/json"
        }
    
    def export_to_jira(self, narrative_id: int, project_key: str, 
                       create_epics: bool = True, create_sprints: bool = False) -> Dict:
        """
        Export project plan to Jira
        
        Args:
            narrative_id: Project ID
            project_key: Jira project key (e.g., 'PROJ')
            create_epics: Create epics for components
            create_sprints: Create sprints based on milestones
        
        Returns:
            Dictionary with created issue keys
        """
        print(f"📤 Exporting project {narrative_id} to Jira project {project_key}...")
        
        results = {
            'epics': [],
            'stories': [],
            'tasks': [],
            'bugs': [],
            'errors': []
        }
        
        # Get project info
        cursor = self.conn.cursor()
        cursor.execute("SELECT title, description FROM product_narratives WHERE narrative_id = ?", 
                      (narrative_id,))
        project_title, project_desc = cursor.fetchone()
        
        # Create epics for components (if enabled)
        component_epic_map = {}
        if create_epics:
            cursor.execute("""
                SELECT component_id, name, description, status, complexity_estimate
                FROM components WHERE narrative_id = ?
            """, (narrative_id,))
            
            for comp_id, name, desc, status, complexity in cursor.fetchall():
                epic_key = self.create_epic(
                    project_key,
                    name,
                    desc,
                    labels=[status, complexity, 'component']
                )
                
                if epic_key:
                    component_epic_map[comp_id] = epic_key
                    results['epics'].append(epic_key)
                    print(f"  ✅ Created epic: {epic_key} - {name}")
        
        # Create issues for development tasks
        cursor.execute("""
            SELECT task_id, component_id, title, description, task_type,
                   priority, estimated_hours, dependencies
            FROM development_tasks
            WHERE narrative_id = ?
            ORDER BY priority, task_id
        """, (narrative_id,))
        
        task_jira_map = {}  # Map task_id to Jira issue key
        
        for task_id, comp_id, title, desc, task_type, priority, hours, deps_json in cursor.fetchall():
            # Determine issue type
            if task_type == 'testing':
                issue_type = 'Task'  # or 'Test' if your Jira has it
            elif task_type == 'documentation':
                issue_type = 'Task'
            else:
                issue_type = 'Story'
            
            # Map priority
            jira_priority = self.map_priority(priority)
            
            # Get epic link
            epic_link = component_epic_map.get(comp_id)
            
            # Create issue
            issue_key = self.create_issue(
                project_key,
                issue_type,
                title,
                desc,
                priority=jira_priority,
                epic_link=epic_link,
                story_points=self.hours_to_story_points(hours),
                labels=[task_type]
            )
            
            if issue_key:
                task_jira_map[task_id] = issue_key
                results['stories' if issue_type == 'Story' else 'tasks'].append(issue_key)
                print(f"  ✅ Created {issue_type}: {issue_key} - {title}")
            else:
                results['errors'].append(f"Failed to create task: {title}")
        
        # Create issue links for dependencies
        dependencies = json.loads(deps_json) if deps_json else []
        for dep_task_id in dependencies:
            if dep_task_id in task_jira_map and task_id in task_jira_map:
                self.create_issue_link(
                    task_jira_map[task_id],
                    task_jira_map[dep_task_id],
                    'Blocks'
                )
        
        # Create issues for gaps
        cursor.execute("""
            SELECT g.gap_id, g.component_id, g.gap_type, g.description, g.impact, g.suggested_solution
            FROM component_gaps g
            JOIN components c ON g.component_id = c.component_id
            WHERE c.narrative_id = ?
        """, (narrative_id,))
        
        for gap_id, comp_id, gap_type, gap_desc, impact, solution in cursor.fetchall():
            epic_link = component_epic_map.get(comp_id)
            
            issue_key = self.create_issue(
                project_key,
                'Bug' if gap_type in ['missing_feature', 'incomplete_implementation'] else 'Task',
                f"Gap: {gap_desc[:100]}",
                f"{gap_desc}\n\nSuggested Solution:\n{solution}",
                priority=self.map_impact_to_priority(impact),
                epic_link=epic_link,
                labels=[gap_type, impact]
            )
            
            if issue_key:
                results['bugs'].append(issue_key)
                print(f"  ✅ Created gap issue: {issue_key}")
        
        # Create sprints based on milestones (if enabled)
        if create_sprints:
            self.create_sprints_from_milestones(narrative_id, project_key)
        
        print(f"\n✅ Export complete!")
        print(f"   Epics: {len(results['epics'])}")
        print(f"   Stories: {len(results['stories'])}")
        print(f"   Tasks: {len(results['tasks'])}")
        print(f"   Bugs: {len(results['bugs'])}")
        print(f"   Errors: {len(results['errors'])}")
        
        return results
    
    def create_epic(self, project_key: str, name: str, description: str, 
                    labels: List[str] = None) -> str:
        """Create Jira epic"""
        payload = {
            "fields": {
                "project": {"key": project_key},
                "summary": name,
                "description": description,
                "issuetype": {"name": "Epic"},
                "labels": labels or []
            }
        }
        
        # Add epic name (custom field - may vary by Jira instance)
        # Common custom field IDs: customfield_10011, customfield_10004
        payload["fields"]["customfield_10011"] = name  # Adjust field ID as needed
        
        try:
            response = requests.post(
                f"{self.jira_url}/rest/api/3/issue",
                json=payload,
                headers=self.headers,
                auth=self.auth,
                timeout=30
            )
            
            if response.status_code == 201:
                return response.json()['key']
            else:
                print(f"Error creating epic: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            print(f"Exception creating epic: {e}")
            return None
    
    def create_issue(self, project_key: str, issue_type: str, summary: str,
                     description: str, priority: str = 'Medium', epic_link: str = None,
                     story_points: int = None, labels: List[str] = None) -> str:
        """Create Jira issue"""
        payload = {
            "fields": {
                "project": {"key": project_key},
                "summary": summary,
                "description": {
                    "type": "doc",
                    "version": 1,
                    "content": [
                        {
                            "type": "paragraph",
                            "content": [
                                {
                                    "type": "text",
                                    "text": description
                                }
                            ]
                        }
                    ]
                },
                "issuetype": {"name": issue_type},
                "priority": {"name": priority},
                "labels": labels or []
            }
        }
        
        # Add epic link if provided
        if epic_link:
            payload["fields"]["parent"] = {"key": epic_link}
        
        # Add story points if provided (custom field - adjust ID as needed)
        if story_points:
            payload["fields"]["customfield_10016"] = story_points  # Adjust field ID
        
        try:
            response = requests.post(
                f"{self.jira_url}/rest/api/3/issue",
                json=payload,
                headers=self.headers,
                auth=self.auth,
                timeout=30
            )
            
            if response.status_code == 201:
                return response.json()['key']
            else:
                print(f"Error creating issue: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            print(f"Exception creating issue: {e}")
            return None
    
    def create_issue_link(self, inward_issue: str, outward_issue: str, 
                          link_type: str = 'Blocks'):
        """Create link between issues"""
        payload = {
            "type": {"name": link_type},
            "inwardIssue": {"key": inward_issue},
            "outwardIssue": {"key": outward_issue}
        }
        
        try:
            response = requests.post(
                f"{self.jira_url}/rest/api/3/issueLink",
                json=payload,
                headers=self.headers,
                auth=self.auth,
                timeout=30
            )
            
            if response.status_code != 201:
                print(f"Error creating link: {response.status_code}")
        except Exception as e:
            print(f"Exception creating link: {e}")
    
    def create_sprints_from_milestones(self, narrative_id: int, project_key: str):
        """Create Jira sprints based on project milestones"""
        # Note: Sprint creation requires Jira Software and board ID
        # This is a simplified version
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT name, description, target_date
            FROM project_milestones
            WHERE narrative_id = ?
            ORDER BY milestone_id
        """, (narrative_id,))
        
        # Sprint creation would go here
        # Requires board ID and additional API calls
        print("  ℹ️  Sprint creation requires additional Jira Software configuration")
    
    def map_priority(self, priority: int) -> str:
        """Map numeric priority to Jira priority"""
        priority_map = {
            1: 'Highest',
            2: 'High',
            3: 'Medium',
            4: 'Low',
            5: 'Lowest'
        }
        return priority_map.get(priority, 'Medium')
    
    def map_impact_to_priority(self, impact: str) -> str:
        """Map impact to Jira priority"""
        impact_map = {
            'blocker': 'Highest',
            'critical': 'Highest',
            'important': 'High',
            'nice_to_have': 'Low'
        }
        return impact_map.get(impact, 'Medium')
    
    def hours_to_story_points(self, hours: float) -> int:
        """Convert estimated hours to story points (Fibonacci scale)"""
        if hours <= 2:
            return 1
        elif hours <= 4:
            return 2
        elif hours <= 8:
            return 3
        elif hours <= 16:
            return 5
        elif hours <= 24:
            return 8
        elif hours <= 40:
            return 13
        else:
            return 21

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: python jira_integration.py <narrative_id> <project_key>")
        print("\nEnvironment variables required:")
        print("  JIRA_URL - Your Jira instance URL")
        print("  JIRA_EMAIL - Your Jira email")
        print("  JIRA_API_TOKEN - Your Jira API token")
        sys.exit(1)
    
    narrative_id = int(sys.argv[1])
    project_key = sys.argv[2]
    
    jira_url = os.getenv('JIRA_URL')
    jira_email = os.getenv('JIRA_EMAIL')
    jira_token = os.getenv('JIRA_API_TOKEN')
    
    if not all([jira_url, jira_email, jira_token]):
        print("❌ Missing required environment variables")
        sys.exit(1)
    
    jira = JiraIntegration(jira_url, jira_email, jira_token)
    results = jira.export_to_jira(narrative_id, project_key, create_epics=True)
    
    print(f"\n✅ Exported to Jira project: {project_key}")
```

---

## 4. GitHub Integration

```python
#!/usr/bin/env python3
"""
github_integration.py - Export project plan to GitHub Issues and Projects
"""
import sqlite3
import json
import requests
from typing import Dict, List
import os

class GitHubIntegration:
    def __init__(self, token: str, owner: str, repo: str, db_path="project_planning.db"):
        """
        Initialize GitHub integration
        
        Args:
            token: GitHub personal access token
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
            create_project: Create GitHub Project board
            create_milestones: Create GitHub milestones
        
        Returns:
            Dictionary with created issue numbers
        """
        print(f"📤 Exporting project {narrative_id} to GitHub {self.owner}/{self.repo}...")
        
        results = {
            'project_id': None,
            'milestones': [],
            'issues': [],
            'errors': []
        }
        
        # Get project info
        cursor = self.conn.cursor()
        cursor.execute("SELECT title, description FROM product_narratives WHERE narrative_id = ?",
                      (narrative_id,))
        project_title, project_desc = cursor.fetchone()
        
        # Create GitHub Project (if enabled)
        if create_project:
            project_id = self.create_project(project_title, project_desc)
            if project_id:
                results['project_id'] = project_id
                print(f"  ✅ Created project: {project_id}")
        
        # Create milestones (if enabled)
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
        
        # Create issues for components
        cursor.execute("""
            SELECT component_id, name, description, status, complexity_estimate
            FROM components WHERE narrative_id = ?
        """, (narrative_id,))
        
        component_issue_map = {}
        
        for comp_id, name, desc, status, complexity in cursor.fetchall():
            labels = ['component', status, complexity]
            
            issue_number = self.create_issue(
                f"Component: {name}",
                desc,
                labels=labels
            )
            
            if issue_number:
                component_issue_map[comp_id] = issue_number
                results['issues'].append(issue_number)
                print(f"  ✅ Created issue #{issue_number}: {name}")
        
        # Create issues for development tasks
        cursor.execute("""
            SELECT task_id, component_id, title, description, task_type,
                   priority, estimated_hours, dependencies
            FROM development_tasks
            WHERE narrative_id = ?
            ORDER BY priority, task_id
        """, (narrative_id,))
        
        task_issue_map = {}
        
        for task_id, comp_id, title, desc, task_type, priority, hours, deps_json in cursor.fetchall():
            labels = [task_type, f'priority-{priority}']
            
            # Add component reference in description
            full_desc = desc
            if comp_id and comp_id in component_issue_map:
                full_desc = f"Related to #{component_issue_map[comp_id]}\n\n{desc}"
            
            full_desc += f"\n\n**Estimated effort:** {hours:.1f} hours"
            
            issue_number = self.create_issue(
                title,
                full_desc,
                labels=labels
            )
            
            if issue_number:
                task_issue_map[task_id] = issue_number
                results['issues'].append(issue_number)
                print(f"  ✅ Created issue #{issue_number}: {title}")
                
                # Add dependencies as comments
                dependencies = json.loads(deps_json) if deps_json else []
                if dependencies:
                    dep_refs = [f"#{task_issue_map[dep_id]}" for dep_id in dependencies 
                               if dep_id in task_issue_map]
                    if dep_refs:
                        self.add_comment(
                            issue_number,
                            f"**Dependencies:** {', '.join(dep_refs)}"
                        )
        
        # Create issues for gaps
        cursor.execute("""
            SELECT g.gap_id, g.component_id, g.gap_type, g.description, g.impact
            FROM component_gaps g
            JOIN components c ON g.component_id = c.component_id
            WHERE c.narrative_id = ?
        """, (narrative_id,))
        
        for gap_id, comp_id, gap_type, gap_desc, impact in cursor.fetchall():
            labels = ['gap', gap_type, impact]
            
            full_desc = gap_desc
            if comp_id and comp_id in component_issue_map:
                full_desc = f"Related to #{component_issue_map[comp_id]}\n\n{gap_desc}"
            
            issue_number = self.create_issue(
                f"Gap: {gap_desc[:80]}",
                full_desc,
                labels=labels
            )
            
            if issue_number:
                results['issues'].append(issue_number)
                print(f"  ✅ Created gap issue #{issue_number}")
        
        print(f"\n✅ Export complete!")
        print(f"   Project ID: {results['project_id']}")
        print(f"   Milestones: {len(results['milestones'])}")
        print(f"   Issues: {len(results['issues'])}")
        
        return results
    
    def create_project(self, name: str, description: str) -> int:
        """Create GitHub Project (v2)"""
        # Note: Projects v2 API requires different authentication
        # This is a simplified version using Projects (Classic)
        
        payload = {
            "name": name,
            "body": description
        }
        
        try:
            response = requests.post(
                f"{self.base_url}/repos/{self.owner}/{self.repo}/projects",
                json=payload,
                headers={**self.headers, "Accept": "application/vnd.github
