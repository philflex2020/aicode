**Excellent!** This is a sophisticated workflow that combines requirements analysis, architecture planning, and project management. Here's a comprehensive system to go from product narrative → component identification → gap analysis → development plan.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│  Input: Product Narrative                               │
│  - Natural language description of product              │
│  - User stories, features, requirements                 │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  Component Extraction & Architecture Design             │
│  - Identify required components                         │
│  - Define interfaces and dependencies                   │
│  - Map to existing code library                         │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  Gap Analysis                                           │
│  - Compare required vs existing components              │
│  - Identify missing functionality                       │
│  - Estimate complexity and effort                       │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  Project Plan Generation                                │
│  - Development tasks with dependencies                  │
│  - Test plan and acceptance criteria                    │
│  - Timeline and milestones                              │
└─────────────────────────────────────────────────────────┘
```

---

## 1. Database Schema for Project Planning

```sql
-- product_narratives table
CREATE TABLE product_narratives (
    narrative_id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    status TEXT DEFAULT 'draft' -- draft, analyzed, planned, in_progress, completed
);

-- requirements table
CREATE TABLE requirements (
    requirement_id INTEGER PRIMARY KEY AUTOINCREMENT,
    narrative_id INTEGER,
    requirement_type TEXT, -- functional, non-functional, constraint
    description TEXT,
    priority TEXT, -- critical, high, medium, low
    acceptance_criteria TEXT,
    FOREIGN KEY (narrative_id) REFERENCES product_narratives(narrative_id)
);

-- components table (architectural components)
CREATE TABLE components (
    component_id INTEGER PRIMARY KEY AUTOINCREMENT,
    narrative_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    component_type TEXT, -- service, library, module, database, api, ui
    responsibilities TEXT, -- what this component does
    interfaces TEXT, -- JSON: exposed APIs/interfaces
    dependencies TEXT, -- JSON: list of component_ids this depends on
    status TEXT, -- exists, partial, missing, deprecated
    existing_code_path TEXT, -- path to existing implementation
    complexity_estimate TEXT, -- trivial, simple, moderate, complex, very_complex
    effort_estimate_hours REAL,
    FOREIGN KEY (narrative_id) REFERENCES product_narratives(narrative_id)
);

-- component_gaps table
CREATE TABLE component_gaps (
    gap_id INTEGER PRIMARY KEY AUTOINCREMENT,
    component_id INTEGER,
    gap_type TEXT, -- missing_component, missing_feature, incomplete_implementation, tech_debt
    description TEXT,
    impact TEXT, -- blocker, critical, important, nice_to_have
    suggested_solution TEXT,
    FOREIGN KEY (component_id) REFERENCES components(component_id)
);

-- development_tasks table
CREATE TABLE development_tasks (
    task_id INTEGER PRIMARY KEY AUTOINCREMENT,
    narrative_id INTEGER,
    component_id INTEGER,
    gap_id INTEGER,
    title TEXT NOT NULL,
    description TEXT,
    task_type TEXT, -- design, implementation, testing, documentation, integration
    priority INTEGER, -- 1 (highest) to 5 (lowest)
    status TEXT DEFAULT 'todo', -- todo, in_progress, blocked, review, done
    estimated_hours REAL,
    actual_hours REAL,
    dependencies TEXT, -- JSON: list of task_ids that must complete first
    assigned_to TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    completed_at DATETIME,
    FOREIGN KEY (narrative_id) REFERENCES product_narratives(narrative_id),
    FOREIGN KEY (component_id) REFERENCES components(component_id),
    FOREIGN KEY (gap_id) REFERENCES component_gaps(gap_id)
);

-- test_plans table
CREATE TABLE test_plans (
    test_plan_id INTEGER PRIMARY KEY AUTOINCREMENT,
    narrative_id INTEGER,
    component_id INTEGER,
    test_type TEXT, -- unit, integration, system, acceptance, performance, security
    test_description TEXT,
    test_cases TEXT, -- JSON: list of test cases
    acceptance_criteria TEXT,
    priority TEXT,
    status TEXT DEFAULT 'pending', -- pending, in_progress, passed, failed
    FOREIGN KEY (narrative_id) REFERENCES product_narratives(narrative_id),
    FOREIGN KEY (component_id) REFERENCES components(component_id)
);

-- project_milestones table
CREATE TABLE project_milestones (
    milestone_id INTEGER PRIMARY KEY AUTOINCREMENT,
    narrative_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    target_date DATE,
    completion_criteria TEXT,
    status TEXT DEFAULT 'pending', -- pending, in_progress, completed, delayed
    dependencies TEXT, -- JSON: list of task_ids
    FOREIGN KEY (narrative_id) REFERENCES product_narratives(narrative_id)
);
```

---

## 2. Product Narrative Analyzer

```python
#!/usr/bin/env python3
"""
narrative_analyzer.py - Extract components and requirements from product narrative
"""
import sqlite3
import json
import requests
from typing import List, Dict, Tuple
from pathlib import Path
import os

class NarrativeAnalyzer:
    def __init__(self, llm_endpoint="http://localhost:8080", db_path="project_planning.db"):
        self.llm_endpoint = llm_endpoint
        self.conn = sqlite3.connect(db_path)
        self.init_db()
    
    def init_db(self):
        """Initialize database schema"""
        schema_path = Path("project_schema.sql")
        if schema_path.exists():
            with open(schema_path, 'r') as f:
                self.conn.executescript(f.read())
            self.conn.commit()
    
    def analyze_narrative(self, narrative_text: str, title: str = None) -> int:
        """
        Main entry point: analyze product narrative and populate database
        Returns narrative_id
        """
        print("📝 Analyzing product narrative...")
        
        # 1. Store narrative
        narrative_id = self.store_narrative(narrative_text, title)
        
        # 2. Extract requirements
        print("🔍 Extracting requirements...")
        requirements = self.extract_requirements(narrative_text)
        self.store_requirements(narrative_id, requirements)
        
        # 3. Identify components
        print("🏗️  Identifying architectural components...")
        components = self.identify_components(narrative_text, requirements)
        self.store_components(narrative_id, components)
        
        # 4. Map to existing code
        print("📂 Mapping to existing code library...")
        self.map_existing_code(narrative_id)
        
        # 5. Identify gaps
        print("🔎 Identifying gaps...")
        self.identify_gaps(narrative_id)
        
        # 6. Generate development plan
        print("📋 Generating development plan...")
        self.generate_development_plan(narrative_id)
        
        # 7. Generate test plan
        print("🧪 Generating test plan...")
        self.generate_test_plan(narrative_id)
        
        # 8. Create milestones
        print("🎯 Creating project milestones...")
        self.create_milestones(narrative_id)
        
        print(f"✅ Analysis complete! Narrative ID: {narrative_id}")
        return narrative_id
    
    def store_narrative(self, text: str, title: str = None) -> int:
        """Store product narrative in database"""
        if not title:
            # Generate title from narrative
            title = self.llm_complete(
                f"Generate a concise title (5-10 words) for this product:\n\n{text[:1000]}\n\nTitle:",
                max_tokens=20
            ).strip()
        
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO product_narratives (title, description, status)
            VALUES (?, ?, 'draft')
        """, (title, text))
        self.conn.commit()
        return cursor.lastrowid
    
    def extract_requirements(self, narrative: str) -> List[Dict]:
        """Extract structured requirements from narrative"""
        prompt = f"""Analyze this product narrative and extract ALL requirements as a structured JSON list.

Product Narrative:
{narrative}

Extract requirements in this exact JSON format:
[
  {{
    "type": "functional|non-functional|constraint",
    "description": "clear, testable requirement statement",
    "priority": "critical|high|medium|low",
    "acceptance_criteria": "how to verify this requirement is met"
  }}
]

Requirements JSON:"""
        
        response = self.llm_complete(prompt, max_tokens=2048, temperature=0.3)
        
        try:
            # Extract JSON from response
            json_start = response.find('[')
            json_end = response.rfind(']') + 1
            if json_start >= 0 and json_end > json_start:
                requirements = json.loads(response[json_start:json_end])
                return requirements
        except json.JSONDecodeError as e:
            print(f"Warning: Failed to parse requirements JSON: {e}")
            # Fallback: extract requirements as text
            return self.extract_requirements_fallback(narrative)
        
        return []
    
    def extract_requirements_fallback(self, narrative: str) -> List[Dict]:
        """Fallback method to extract requirements as structured text"""
        prompt = f"""List all functional and non-functional requirements from this product narrative.
Format each as:
- [PRIORITY] Requirement description | Acceptance criteria

Product Narrative:
{narrative}

Requirements:"""
        
        response = self.llm_complete(prompt, max_tokens=1024)
        
        # Parse text-based requirements
        requirements = []
        for line in response.split('\n'):
            line = line.strip()
            if line.startswith('-') or line.startswith('*'):
                # Simple parsing
                requirements.append({
                    'type': 'functional',
                    'description': line[1:].strip(),
                    'priority': 'medium',
                    'acceptance_criteria': 'To be defined'
                })
        
        return requirements
    
    def store_requirements(self, narrative_id: int, requirements: List[Dict]):
        """Store requirements in database"""
        cursor = self.conn.cursor()
        for req in requirements:
            cursor.execute("""
                INSERT INTO requirements 
                (narrative_id, requirement_type, description, priority, acceptance_criteria)
                VALUES (?, ?, ?, ?, ?)
            """, (
                narrative_id,
                req.get('type', 'functional'),
                req.get('description', ''),
                req.get('priority', 'medium'),
                req.get('acceptance_criteria', '')
            ))
        self.conn.commit()
    
    def identify_components(self, narrative: str, requirements: List[Dict]) -> List[Dict]:
        """Identify architectural components needed"""
        req_summary = "\n".join([f"- {r['description']}" for r in requirements[:20]])
        
        prompt = f"""You are a software architect. Design the system architecture for this product.

Product Narrative:
{narrative[:2000]}

Key Requirements:
{req_summary}

Identify ALL major components needed. For each component, provide:
1. Component name (concise, technical)
2. Type (service, library, module, database, api, ui, infrastructure)
3. Responsibilities (what it does)
4. Interfaces (APIs it exposes)
5. Dependencies (what it depends on)
6. Complexity estimate (trivial, simple, moderate, complex, very_complex)

Output as JSON array:
[
  {{
    "name": "ComponentName",
    "type": "service|library|module|database|api|ui|infrastructure",
    "description": "brief description",
    "responsibilities": ["responsibility 1", "responsibility 2"],
    "interfaces": ["interface 1", "interface 2"],
    "dependencies": ["ComponentA", "ComponentB"],
    "complexity": "moderate"
  }}
]

Components JSON:"""
        
        response = self.llm_complete(prompt, max_tokens=3072, temperature=0.4)
        
        try:
            json_start = response.find('[')
            json_end = response.rfind(']') + 1
            if json_start >= 0 and json_end > json_start:
                components = json.loads(response[json_start:json_end])
                return components
        except json.JSONDecodeError as e:
            print(f"Warning: Failed to parse components JSON: {e}")
            return []
        
        return []
    
    def store_components(self, narrative_id: int, components: List[Dict]):
        """Store components in database"""
        cursor = self.conn.cursor()
        
        complexity_to_hours = {
            'trivial': 4,
            'simple': 16,
            'moderate': 40,
            'complex': 80,
            'very_complex': 160
        }
        
        for comp in components:
            complexity = comp.get('complexity', 'moderate')
            effort = complexity_to_hours.get(complexity, 40)
            
            cursor.execute("""
                INSERT INTO components
                (narrative_id, name, description, component_type, responsibilities,
                 interfaces, dependencies, status, complexity_estimate, effort_estimate_hours)
                VALUES (?, ?, ?, ?, ?, ?, ?, 'missing', ?, ?)
            """, (
                narrative_id,
                comp.get('name', 'Unknown'),
                comp.get('description', ''),
                comp.get('type', 'module'),
                json.dumps(comp.get('responsibilities', [])),
                json.dumps(comp.get('interfaces', [])),
                json.dumps(comp.get('dependencies', [])),
                complexity,
                effort
            ))
        
        self.conn.commit()
    
    def map_existing_code(self, narrative_id: int):
        """Map components to existing code in library"""
        cursor = self.conn.cursor()
        
        # Get all components
        cursor.execute("""
            SELECT component_id, name, description, responsibilities
            FROM components
            WHERE narrative_id = ?
        """, (narrative_id,))
        
        components = cursor.fetchall()
        
        # Get existing code library (scan directories)
        code_library = self.scan_code_library()
        
        for comp_id, name, desc, resp_json in components:
            # Use LLM to find best match in existing code
            match = self.find_code_match(name, desc, resp_json, code_library)
            
            if match:
                status = match['status']  # 'exists', 'partial', or 'missing'
                code_path = match.get('path', None)
                
                cursor.execute("""
                    UPDATE components
                    SET status = ?, existing_code_path = ?
                    WHERE component_id = ?
                """, (status, code_path, comp_id))
        
        self.conn.commit()
    
    def scan_code_library(self) -> List[Dict]:
        """Scan code library and create index"""
        code_library = []
        
        # Define directories to scan
        scan_dirs = [
            os.path.expanduser("~/code_library"),
            os.path.expanduser("~/projects"),
            "/mnt/nvme/work"
        ]
        
        for base_dir in scan_dirs:
            if not os.path.exists(base_dir):
                continue
            
            for root, dirs, files in os.walk(base_dir):
                # Skip hidden and build directories
                dirs[:] = [d for d in dirs if not d.startswith('.') and d not in ['build', 'node_modules', '__pycache__']]
                
                for file in files:
                    if file.endswith(('.py', '.cpp', '.h', '.hpp', '.js', '.ts', '.go', '.rs')):
                        file_path = os.path.join(root, file)
                        try:
                            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                                content = f.read(5000)  # First 5KB
                            
                            code_library.append({
                                'path': file_path,
                                'filename': file,
                                'content_preview': content
                            })
                        except Exception as e:
                            continue
        
        return code_library
    
    def find_code_match(self, comp_name: str, comp_desc: str, 
                        responsibilities: str, code_library: List[Dict]) -> Dict:
        """Use LLM to find matching code in library"""
        if not code_library:
            return {'status': 'missing', 'path': None}
        
        # Sample up to 20 files for matching
        sample = code_library[:20]
        
        code_samples = "\n\n".join([
            f"File: {item['filename']}\nPath: {item['path']}\nPreview:\n{item['content_preview'][:500]}"
            for item in sample
        ])
        
        prompt = f"""Analyze if any existing code matches this component:

Component: {comp_name}
Description: {comp_desc}
Responsibilities: {responsibilities}

Existing Code Library:
{code_samples}

Does any existing code implement this component?
- If YES and fully implements it: respond "EXISTS: <file_path>"
- If YES but partially implements it: respond "PARTIAL: <file_path>"
- If NO match found: respond "MISSING"

Response:"""
        
        response = self.llm_complete(prompt, max_tokens=50, temperature=0.2)
        
        if response.startswith("EXISTS:"):
            return {'status': 'exists', 'path': response.split(":", 1)[1].strip()}
        elif response.startswith("PARTIAL:"):
            return {'status': 'partial', 'path': response.split(":", 1)[1].strip()}
        else:
            return {'status': 'missing', 'path': None}
    
    def identify_gaps(self, narrative_id: int):
        """Identify gaps in components"""
        cursor = self.conn.cursor()
        
        # Get components that are missing or partial
        cursor.execute("""
            SELECT component_id, name, description, status, responsibilities
            FROM components
            WHERE narrative_id = ? AND status IN ('missing', 'partial')
        """, (narrative_id,))
        
        components = cursor.fetchall()
        
        for comp_id, name, desc, status, resp_json in components:
            if status == 'missing':
                # Entire component is missing
                cursor.execute("""
                    INSERT INTO component_gaps
                    (component_id, gap_type, description, impact, suggested_solution)
                    VALUES (?, 'missing_component', ?, 'blocker', ?)
                """, (
                    comp_id,
                    f"Component '{name}' needs to be implemented from scratch",
                    f"Implement {name} with responsibilities: {resp_json}"
                ))
            
            elif status == 'partial':
                # Identify what's missing in partial implementation
                gaps = self.analyze_partial_component(comp_id, name, desc, resp_json)
                
                for gap in gaps:
                    cursor.execute("""
                        INSERT INTO component_gaps
                        (component_id, gap_type, description, impact, suggested_solution)
                        VALUES (?, ?, ?, ?, ?)
                    """, (
                        comp_id,
                        gap['type'],
                        gap['description'],
                        gap['impact'],
                        gap['solution']
                    ))
        
        self.conn.commit()
    
    def analyze_partial_component(self, comp_id: int, name: str, 
                                   desc: str, responsibilities: str) -> List[Dict]:
        """Analyze what's missing in a partial component"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT existing_code_path FROM components WHERE component_id = ?", (comp_id,))
        code_path = cursor.fetchone()[0]
        
        if not code_path or not os.path.exists(code_path):
            return []
        
        # Read existing code
        with open(code_path, 'r', encoding='utf-8', errors='ignore') as f:
            existing_code = f.read(10000)
        
        prompt = f"""Compare the required component with existing implementation:

Required Component: {name}
Description: {desc}
Required Responsibilities: {responsibilities}

Existing Implementation:
{existing_code}

Identify what's MISSING or INCOMPLETE. List each gap as:
- Gap type: missing_feature | incomplete_implementation | tech_debt
- Description: what's missing
- Impact: blocker | critical | important | nice_to_have
- Solution: how to fix it

Format as JSON array:
[{{"type": "...", "description": "...", "impact": "...", "solution": "..."}}]

Gaps JSON:"""
        
        response = self.llm_complete(prompt, max_tokens=1024, temperature=0.3)
        
        try:
            json_start = response.find('[')
            json_end = response.rfind(']') + 1
            if json_start >= 0 and json_end > json_start:
                return json.loads(response[json_start:json_end])
        except:
            pass
        
        return []
    
    def generate_development_plan(self, narrative_id: int):
        """Generate development tasks from components and gaps"""
        cursor = self.conn.cursor()
        
        # Get all components and their gaps
        cursor.execute("""
            SELECT c.component_id, c.name, c.description, c.status, 
                   c.complexity_estimate, c.effort_estimate_hours,
                   g.gap_id, g.gap_type, g.description, g.impact, g.suggested_solution
            FROM components c
            LEFT JOIN component_gaps g ON c.component_id = g.component_id
            WHERE c.narrative_id = ?
            ORDER BY c.component_id
        """, (narrative_id,))
        
        rows = cursor.fetchall()
        
        # Group by component
        components_with_gaps = {}
        for row in rows:
            comp_id = row[0]
            if comp_id not in components_with_gaps:
                components_with_gaps[comp_id] = {
                    'component': row[:6],
                    'gaps': []
                }
            if row[6]:  # gap_id exists
                components_with_gaps[comp_id]['gaps'].append(row[6:])
        
        # Generate tasks for each component
        task_priority = 1
        
        for comp_id, data in components_with_gaps.items():
            comp = data['component']
            gaps = data['gaps']
            
            comp_name = comp[1]
            status = comp[3]
            effort = comp[5]
            
            if status == 'missing':
                # Create design + implementation + testing tasks
                tasks = [
                    {
                        'title': f"Design {comp_name} architecture",
                        'description': f"Design detailed architecture for {comp_name}",
                        'type': 'design',
                        'hours': effort * 0.15
                    },
                    {
                        'title': f"Implement {comp_name}",
                        'description': f"Implement {comp_name} according to design",
                        'type': 'implementation',
                        'hours': effort * 0.60
                    },
                    {
                        'title': f"Unit tests for {comp_name}",
                        'description': f"Write comprehensive unit tests",
                        'type': 'testing',
                        'hours': effort * 0.15
                    },
                    {
                        'title': f"Document {comp_name}",
                        'description': f"Write API documentation and usage examples",
                        'type': 'documentation',
                        'hours': effort * 0.10
                    }
                ]
                
                for task in tasks:
                    cursor.execute("""
                        INSERT INTO development_tasks
                        (narrative_id, component_id, title, description, task_type, 
                         priority, estimated_hours)
                        VALUES (?, ?, ?, ?, ?, ?, ?)
                    """, (
                        narrative_id, comp_id, task['title'], task['description'],
                        task['type'], task_priority, task['hours']
                    ))
                
                task_priority += 1
            
            elif status == 'partial':
                # Create tasks for each gap
                for gap in gaps:
                    gap_id, gap_type, gap_desc, impact, solution = gap
                    
                    priority_map = {'blocker': 1, 'critical': 2, 'important': 3, 'nice_to_have': 4}
                    priority = priority_map.get(impact, 3)
                    
                    cursor.execute("""
                        INSERT INTO development_tasks
                        (narrative_id, component_id, gap_id, title, description, 
                         task_type, priority, estimated_hours)
                        VALUES (?, ?, ?, ?, ?, 'implementation', ?, ?)
                    """, (
                        narrative_id, comp_id, gap_id,
                        f"Fix: {gap_desc[:50]}",
                        solution,
                        priority,
                        effort * 0.3  # Estimate 30% of component effort per gap
                    ))
        
        self.conn.commit()
    
    def generate_test_plan(self, narrative_id: int):
        """Generate comprehensive test plan"""
        cursor = self.conn.cursor()
        
        # Get all components
        cursor.execute("""
            SELECT component_id, name, description, component_type, responsibilities
            FROM components
            WHERE narrative_id = ?
        """, (narrative_id,))
        
        components = cursor.fetchall()
        
        for comp_id, name, desc, comp_type, resp_json in components:
            # Generate test cases using LLM
            test_cases = self.generate_test_cases(name, desc, resp_json)
            
            # Unit tests
            cursor.execute("""
                INSERT INTO test_plans
                (narrative_id, component_id, test_type, test_description, 
                 test_cases, priority)
                VALUES (?, ?, 'unit', ?, ?, 'high')
            """, (
                narrative_id, comp_id,
                f"Unit tests for {name}",
                json.dumps(test_cases.get('unit', []))
            ))
            
            # Integration tests
            cursor.execute("""
                INSERT INTO test_plans
                (narrative_id, component_id, test_type, test_description,
                 test_cases, priority)
                VALUES (?, ?, 'integration', ?, ?, 'high')
            """, (
                narrative_id, comp_id,
                f"Integration tests for {name}",
                json.dumps(test_cases.get('integration', []))
            ))
        
        # System-level tests
        cursor.execute("""
            INSERT INTO test_plans
            (narrative_id, component_id, test_type, test_description, priority)
            VALUES (?, NULL, 'system', 'End-to-end system tests', 'critical')
        """, (narrative_id,))
        
        # Performance tests
        cursor.execute("""
            INSERT INTO test_plans
            (narrative_id, component_id, test_type, test_description, priority)
            VALUES (?, NULL, 'performance', 'Load and stress testing', 'medium')
        """, (narrative_id,))
        
        self.conn.commit()
    
    def generate_test_cases(self, comp_name: str, desc: str, responsibilities: str) -> Dict:
        """Generate test cases for a component"""
        prompt = f"""Generate test cases for this component:

Component: {comp_name}
Description: {desc}
Responsibilities: {responsibilities}

Generate test cases in JSON format:
{{
  "unit": [
    {{"name": "test_case_name", "description": "what it tests", "expected": "expected outcome"}},
    ...
  ],
  "integration": [
    {{"name": "test_case_name", "description": "what it tests", "expected": "expected outcome"}},
    ...
  ]
}}

Test Cases JSON:"""
        
        response = self.llm_complete(prompt, max_tokens=1024, temperature=0.4)
        
        try:
            json_start = response.find('{')
            json_end = response.rfind('}') + 1
            if json_start >= 0 and json_end > json_start:
                return json.loads(response[json_start:json_end])
        except:
            pass
        
        return {'unit': [], 'integration': []}
    
    def create_milestones(self, narrative_id: int):
        """Create project milestones"""
        cursor = self.conn.cursor()
        
        # Get total effort
        cursor.execute("""
            SELECT SUM(estimated_hours) FROM development_tasks
            WHERE narrative_id = ?
        """, (narrative_id,))
        
        total_hours = cursor.fetchone()[0] or 0
        
        # Define milestones (customize based on your methodology)
        milestones = [
            {
                'name': 'Architecture & Design Complete',
                'description': 'All component designs finalized and reviewed',
                'percentage': 0.15
            },
            {
                'name': 'Core Components Implemented',
                'description': 'Critical path components implemented and unit tested',
                'percentage': 0.40
            },
            {
                'name': 'Integration Complete',
                'description': 'All components integrated and integration tests passing',
                'percentage': 0.70
            },
            {
                'name': 'Testing & QA Complete',
                'description': 'All tests passing, performance validated',
                'percentage': 0.90
            },
            {
                'name': 'Production Ready',
                'description': 'Documentation complete, deployment ready',
                'percentage': 1.0
            }
        ]
        
        for milestone in milestones:
            cursor.execute("""
                INSERT INTO project_milestones
                (narrative_id, name, description, completion_criteria)
                VALUES (?, ?, ?, ?)
            """, (
                narrative_id,
                milestone['name'],
                milestone['description'],
                f"{milestone['percentage']*100}% of tasks complete"
            ))
        
        self.conn.commit()
    
    def llm_complete(self, prompt: str, max_tokens: int = 512, temperature: float = 0.7) -> str:
        """Send completion request to LLM"""
        try:
            response = requests.post(
                f"{self.llm_endpoint}/completion",
                json={
                    "prompt": prompt,
                    "n_predict": max_tokens,
                    "temperature": temperature,
                    "stop": ["\n\n\n", "---"]
                },
                timeout=120
            )
            
            if response.status_code == 200:
                return response.json()['content'].strip()
        except Exception as e:
            print(f"LLM error: {e}")
        
        return ""

# CLI interface
if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python narrative_analyzer.py <narrative_file.txt>")
        sys.exit(1)
    
    narrative_file = sys.argv[1]
    
    with open(narrative_file, 'r') as f:
        narrative_text = f.read()
    
    analyzer = NarrativeAnalyzer()
    narrative_id = analyzer.analyze_narrative(narrative_text)
    
    print(f"\n✅ Analysis complete! Narrative ID: {narrative_id}")
    print(f"Run: python generate_project_report.py {narrative_id}")
```

---

## 3. Project Report Generator

```python
#!/usr/bin/env python3
"""
generate_project_report.py - Generate comprehensive project plan report
"""
import sqlite3
import json
from datetime import datetime, timedelta
from pathlib import Path

def generate_project_report(narrative_id: int, output_dir="reports"):
    """Generate comprehensive project report"""
    conn = sqlite3.connect("project_planning.db")
    cursor = conn.cursor()
    
    # Get narrative
    cursor.execute("SELECT title, description FROM product_narratives WHERE narrative_id = ?", (narrative_id,))
    title, description = cursor.fetchone()
    
    # Get requirements
    cursor.execute("""
        SELECT requirement_type, description, priority, acceptance_criteria
        FROM requirements WHERE narrative_id = ?
        ORDER BY 
            CASE priority 
                WHEN 'critical' THEN 1 
                WHEN 'high' THEN 2 
                WHEN 'medium' THEN 3 
                ELSE 4 
            END
    """, (narrative_id,))
    requirements = cursor.fetchall()
    
    # Get components
    cursor.execute("""
        SELECT name, description, component_type, status, 
               complexity_estimate, effort_estimate_hours, existing_code_path
        FROM components WHERE narrative_id = ?
        ORDER BY 
            CASE status 
                WHEN 'missing' THEN 1 
                WHEN 'partial' THEN 2 
                ELSE 3 
            END
    """, (narrative_id,))
    components = cursor.fetchall()
    
    # Get gaps
    cursor.execute("""
        SELECT c.name, g.gap_type, g.description, g.impact, g.suggested_solution
        FROM component_gaps g
        JOIN components c ON g.component_id = c.component_id
        WHERE c.narrative_id = ?
        ORDER BY 
            CASE g.impact 
                WHEN 'blocker' THEN 1 
                WHEN 'critical' THEN 2 
                WHEN 'important' THEN 3 
                ELSE 4 
            END
    """, (narrative_id,))
    gaps = cursor.fetchall()
    
    # Get tasks
    cursor.execute("""
        SELECT title, description, task_type, priority, estimated_hours, status
        FROM development_tasks WHERE narrative_id = ?
        ORDER BY priority, task_id
    """, (narrative_id,))
    tasks = cursor.fetchall()
    
    # Get test plans
    cursor.execute("""
        SELECT test_type, test_description, priority
        FROM test_plans WHERE narrative_id = ?
        ORDER BY 
            CASE priority 
                WHEN 'critical' THEN 1 
                WHEN 'high' THEN 2 
                WHEN 'medium' THEN 3 
                ELSE 4 
            END
    """, (narrative_id,))
    test_plans = cursor.fetchall()
    
    # Get milestones
    cursor.execute("""
        SELECT name, description, completion_criteria
        FROM project_milestones WHERE narrative_id = ?
        ORDER BY milestone_id
    """, (narrative_id,))
    milestones = cursor.fetchall()
    
    # Calculate statistics
    total_effort = sum([t[4] for t in tasks])
    total_tasks = len(tasks)
    missing_components = len([c for c in components if c[3] == 'missing'])
    partial_components = len([c for c in components if c[3] == 'partial'])
    existing_components = len([c for c in components if c[3] == 'exists'])
    
    # Generate HTML report
    Path(output_dir).mkdir(exist_ok=True)
    report_path = Path(output_dir) / f"project_plan_{narrative_id}.html"
    
    html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Project Plan: {title}</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 40px; line-height: 1.6; }}
        h1 {{ color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }}
        h2 {{ color: #34495e; margin-top: 30px; border-bottom: 2px solid #95a5a6; padding-bottom: 5px; }}
        h3 {{ color: #7f8c8d; }}
        table {{ border-collapse: collapse; width: 100%; margin: 20px 0; }}
        th, td {{ border: 1px solid #ddd; padding: 12px; text-align: left; }}
        th {{ background-color: #3498db; color: white; }}
        tr:nth-child(even) {{ background-color: #f2f2f2; }}
        .status-missing {{ background-color: #e74c3c; color: white; padding: 3px 8px; border-radius: 3px; }}
        .status-partial {{ background-color: #f39c12; color: white; padding: 3px 8px; border-radius: 3px; }}
        .status-exists {{ background-color: #27ae60; color: white; padding: 3px 8px; border-radius: 3px; }}
        .priority-critical {{ color: #c0392b; font-weight: bold; }}
        .priority-high {{ color: #e67e22; font-weight: bold; }}
        .priority-medium {{ color: #f39c12; }}
        .priority-low {{ color: #95a5a6; }}
        .summary-box {{ background-color: #ecf0f1; padding: 20px; border-radius: 5px; margin: 20px 0; }}
        .stat {{ display: inline-block; margin: 10px 20px 10px 0; }}
        .stat-value {{ font-size: 2em; font-weight: bold; color: #3498db; }}
        .stat-label {{ font-size: 0.9em; color: #7f8c8d; }}
    </style>
</head>
<body>
    <h1>Project Development Plan: {title}</h1>
    <p><strong>Generated:</strong> {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
    
    <div class="summary-box">
        <h2>Executive Summary</h2>
        <div class="stat">
            <div class="stat-value">{total_tasks}</div>
            <div class="stat-label">Total Tasks</div>
        </div>
        <div class="stat">
            <div class="stat-value">{total_effort:.0f}h</div>
            <div class="stat-label">Estimated Effort</div>
        </div>
        <div class="stat">
            <div class="stat-value">{len(components)}</div>
            <div class="stat-label">Components</div>
        </div>
        <div class="stat">
            <div class="stat-value">{len(gaps)}</div>
            <div class="stat-label">Gaps Identified</div>
        </div>
        <p><strong>Component Status:</strong> 
            {existing_components} existing, 
            {partial_components} partial, 
            {missing_components} missing
        </p>
    </div>
    
    <h2>Product Description</h2>
    <p>{description[:1000]}</p>
    
    <h2>Requirements ({len(requirements)})</h2>
    <table>
        <tr>
            <th>Type</th>
            <th>Priority</th>
            <th>Description</th>
            <th>Acceptance Criteria</th>
        </tr>
"""
    
    for req_type, desc, priority, criteria in requirements:
        priority_class = f"priority-{priority}"
        html += f"""
        <tr>
            <td>{req_type}</td>
            <td class="{priority_class}">{priority.upper()}</td>
            <td>{desc}</td>
            <td>{criteria}</td>
        </tr>
"""
    
    html += """
    </table>
    
    <h2>Architecture Components</h2>
    <table>
        <tr>
            <th>Component</th>
            <th>Type</th>
            <th>Status</th>
            <th>Complexity</th>
            <th>Effort (hours)</th>
            <th>Existing Code</th>
        </tr>
"""
    
    for name, desc, comp_type, status, complexity, effort, code_path in components:
        status_class = f"status-{status}"
        code_display = code_path if code_path else "N/A"
        html += f"""
        <tr>
            <td><strong>{name}</strong><br/><small>{desc}</small></td>
            <td>{comp_type}</td>
            <td><span class="{status_class}">{status.upper()}</span></td>
            <td>{complexity}</td>
            <td>{effort:.0f}</td>
            <td><small>{code_display}</small></td>
        </tr>
"""
    
    html += """
    </table>
    
    <h2>Identified Gaps</h2>
    <table>
        <tr>
            <th>Component</th>
            <th>Gap Type</th>
            <th>Description</th>
            <th>Impact</th>
            <th>Suggested Solution</th>
        </tr>
"""
    
    for comp_name, gap_type, gap_desc, impact, solution in gaps:
        impact_class = f"priority-{impact}" if impact in ['blocker', 'critical'] else ""
        html += f"""
        <tr>
            <td>{comp_name}</td>
            <td>{gap_type.replace('_', ' ').title()}</td>
            <td>{gap_desc}</td>
            <td class="{impact_class}">{impact.upper()}</td>
            <td>{solution}</td>
        </tr>
"""
    
    html += """
    </table>
    
    <h2>Development Tasks</h2>
    <table>
        <tr>
            <th>Priority</th>
            <th>Task</th>
            <th>Type</th>
            <th>Estimated Hours</th>
            <th>Status</th>
        </tr>
"""
    
    for task_title, task_desc, task_type, priority, hours, status in tasks:
        html += f"""
        <tr>
            <td>{priority}</td>
            <td><strong>{task_title}</strong><br/><small>{task_desc}</small></td>
            <td>{task_type}</td>
            <td>{hours:.1f}</td>
            <td>{status}</td>
        </tr>
"""
    
    html += """
    </table>
    
    <h2>Test Plan</h2>
    <table>
        <tr>
            <th>Test Type</th>
            <th>Description</th>
            <th>Priority</th>
        </tr>
"""
    
    for test_type, test_desc, priority in test_plans:
        html += f"""
        <tr>
            <td>{test_type.upper()}</td>
            <td>{test_desc}</td>
            <td>{priority}</td>
        </tr>
"""
    
    html += """
    </table>
    
    <h2>Project Milestones</h2>
    <table>
        <tr>
            <th>Milestone</th>
            <th>Description</th>
            <th>Completion Criteria</th>
        </tr>
"""
    
    for ms_name, ms_desc, criteria in milestones:
        html += f"""
        <tr>
            <td><strong>{ms_name}</strong></td>
            <td>{ms_desc}</td>
            <td>{criteria}</td>
        </tr>
"""
    
    html += """
    </table>
    
    <h2>Timeline Estimate</h2>
    <p>Based on estimated effort and assuming 1 developer working 6 productive hours/day:</p>
    <ul>
"""
    
    days = total_effort / 6
    weeks = days / 5
    
    html += f"""
        <li><strong>Total Effort:</strong> {total_effort:.0f} hours</li>
        <li><strong>Calendar Time (1 dev):</strong> ~{days:.0f} working days (~{weeks:.1f} weeks)</li>
        <li><strong>Suggested Team Size:</strong> {max(1, int(weeks / 12))} developers for 3-month delivery</li>
    </ul>
    
</body>
</html>
"""
    
    report_path.write_text(html)
    print(f"✅ Report generated: {report_path}")
    
    # Also generate JSON export
    json_path = Path(output_dir) / f"project_plan_{narrative_id}.json"
    export_data = {
        'narrative_id': narrative_id,
        'title': title,
        'description': description,
        'requirements': [
            {'type': r[0], 'description': r[1], 'priority': r[2], 'acceptance_criteria': r[3]}
            for r in requirements
        ],
        'components': [
            {'name': c[0], 'description': c[1], 'type': c[2], 'status': c[3], 
             'complexity': c[4], 'effort_hours': c[5], 'existing_code': c[6]}
            for c in components
        ],
        'gaps': [
            {'component': g[0], 'type': g[1], 'description': g[2], 'impact': g[3], 'solution': g[4]}
            for g in gaps
        ],
        'tasks': [
            {'title': t[0], 'description': t[1], 'type': t[2], 'priority': t[3], 
             'estimated_hours': t[4], 'status': t[5]}
            for t in tasks
        ],
        'test_plans': [
            {'type': tp[0], 'description': tp[1], 'priority': tp[2]}
            for tp in test_plans
        ],
        'milestones': [
            {'name': m[0], 'description': m[1], 'criteria': m[2]}
            for m in milestones
        ],
        'statistics': {
            'total_tasks': total_tasks,
            'total_effort_hours': total_effort,
            'total_components': len(components),
            'missing_components': missing_components,
            'partial_components': partial_components,
            'existing_components': existing_components,
            'total_gaps': len(gaps)
        }
    }
    
    json_path.write_text(json.dumps(export_data, indent=2))
    print(f"✅ JSON export: {json_path}")
    
    return report_path

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python generate_project_report.py <narrative_id>")
        sys.exit(1)
    
    narrative_id = int(sys.argv[1])
    generate_project_report(narrative_id)
```

---

## 4. Example Product Narrative

Create a file `example_narrative.txt`:

```
Product Narrative: Real-Time IoT Monitoring Platform

We need to build a real-time monitoring platform for IoT devices deployed in industrial environments. 
The system must collect telemetry data from thousands of sensors, process it in real-time, detect 
anomalies, and provide dashboards for operators.

Key Features:
1. Data Ingestion: Accept data from IoT devices via MQTT, HTTP, and WebSocket protocols
2. Real-time Processing: Process incoming data streams with <100ms latency
3. Anomaly Detection: Use ML models to detect unusual patterns and alert operators
4. Time-Series Storage: Store historical data for analysis and compliance (7-year retention)
5. Dashboards: Web-based dashboards showing real-time metrics and historical trends
6. Alerting: Send alerts via email, SMS, and webhook when anomalies detected
7. Multi-tenancy: Support multiple customers with isolated data and access control
8. High Availability: 99.9% uptime SLA with automatic failover

Non-Functional Requirements:
- Handle 10,000 messages/second sustained, 50,000 peak
- Support 100,000 concurrent device connections
- Query response time <500ms for dashboard queries
- Data must be encrypted at rest and in transit
- Must comply with ISO 27001 and SOC 2
- Horizontal scalability to handle growth

Technical Constraints:
- Must run on-premises (customer requirement)
- Use open-source components where possible
- Support deployment on Kubernetes
- Integrate with existing LDAP for authentication
```

---

## 5. Usage Workflow

```bash
# 1. Start LLM server (on Sparks when they arrive)
./build/bin/llama-server \
  -m ~/models/mixtral-8x7b-instruct.Q4_K_M.gguf \
  --host 0.0.0.0 --port 8080 \
  -ngl 999 --ctx-size 16384 -t $(nproc)

# 2. Analyze product narrative
python3 narrative_analyzer.py example_narrative.txt

# Output:
# 📝 Analyzing product narrative...
# 🔍 Extracting requirements...
# 🏗️  Identifying architectural components...
# 📂 Mapping to existing code library...
# 🔎 Identifying gaps...
# 📋 Generating development plan...
# 🧪 Generating test plan...
# 🎯 Creating project milestones...
# ✅ Analysis complete! Narrative ID: 1

# 3. Generate comprehensive report
python3 generate_project_report.py 1

# Output:
# ✅ Report generated: reports/project_plan_1.html
# ✅ JSON export: reports/project_plan_1.json

# 4. View the report
firefox reports/project_plan_1.html

# 5. Export to project management tool (optional)
python3 export_to_jira.py 1  # or export_to_github_issues.py
```

---

## 6. Integration with Code Generation

Once you have the project plan, integrate with your code generation workflow:

```python
#!/usr/bin/env python3
"""
generate_component_code.py - Generate code for missing components
"""
import sqlite3
import requests

def generate_component_code(component_id: int, code_library_path: str):
    """Generate code for a component using existing library as examples"""
    conn = sqlite3.connect("project_planning.db")
    cursor = conn.cursor()
    
    # Get component details
    cursor.execute("""
        SELECT name, description, responsibilities, interfaces, dependencies
        FROM components WHERE component_id = ?
    """, (component_id,))
    
    name, desc, resp, interfaces, deps = cursor.fetchone()
    
    # Find similar components in library
    similar_code = find_similar_components(name, desc, code_library_path)
    
    # Build prompt with examples
    prompt = f"""Generate production-ready code for this component:

Component: {name}
Description: {desc}
Responsibilities: {resp}
Interfaces: {interfaces}
Dependencies: {deps}

Reference implementations from code library:
{similar_code}

Generate complete, well-documented code with:
- Error handling
- Logging
- Unit tests
- API documentation

Code:"""
    
    # Send to LLM
    response = requests.post(
        "http://localhost:8080/completion",
        json={"prompt": prompt, "n_predict": 4096, "temperature": 0.3}
    )
    
    generated_code = response.json()['content']
    
    # Save to file
    output_path = f"generated/{name.lower().replace(' ', '_')}.py"
    with open(output_path, 'w') as f:
        f.write(generated_code)
    
    print(f"✅ Generated: {output_path}")
    
    return output_path
```

---

## 7. Advanced: Continuous Planning

Monitor project progress and update plan:

```python
def update_project_status(narrative_id: int):
    """Update project status based on completed tasks"""
    conn = sqlite3.connect("project_planning.db")
    cursor = conn.cursor()
    
    # Calculate completion percentage
    cursor.execute("""
        SELECT 
            COUNT(*) as total,
            SUM(CASE WHEN status = 'done' THEN 1 ELSE 0 END) as completed
        FROM development_tasks
        WHERE narrative_id = ?
    """, (narrative_id,))
    
    total, completed = cursor.fetchone()
    completion_pct = (completed / total * 100) if total > 0 else 0
    
    # Update milestones
    cursor.execute("""
        UPDATE project_milestones
        SET status = 'completed'
        WHERE narrative_id = ?
          AND CAST(SUBSTR(completion_criteria, 1, INSTR(completion_criteria, '%')-1) AS REAL) <= ?
    """, (narrative_id, completion_pct))
    
    conn.commit()
    
    print(f"Project {narrative_id}: {completion_pct:.1f}% complete")
```

---

## Summary

This system gives you:

✅ **Automated requirements extraction** from natural language narratives  
✅ **Component identification** and architecture design  
✅ **Gap analysis** against existing code library  
✅ **Detailed development plan** with tasks, estimates, and dependencies  
✅ **Comprehensive test plan** with test cases  
✅ **Project milestones** and timeline estimates  
✅ **HTML and JSON reports** for sharing and tracking  
✅ **Integration with code generation** workflow  

When your Sparks arrive, you'll be able to:
- Process longer, more detailed narratives (128k context)
- Use more sophisticated models (Mixtral 8x7B, CodeLlama 34B)
- Generate more accurate component designs
- Create better code from examples

Let me know if you want me to add specific features like Jira/GitHub integration, Gantt chart generation, or risk analysis!
