import os

project_files = {
    "database.py": '''from sqlalchemy import create_engine
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker

SQLALCHEMY_DATABASE_URL = "sqlite:///./test_system.db"

engine = create_engine(
    SQLALCHEMY_DATABASE_URL, connect_args={"check_same_thread": False}
)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

Base = declarative_base()
''',

    "models.py": '''from sqlalchemy import Column, Integer, String, Text, ForeignKey, DateTime
from sqlalchemy.orm import relationship
from datetime import datetime
from database import Base

class TestDefinition(Base):
    __tablename__ = "test_definitions"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String, unique=True, index=True)
    description = Column(Text, nullable=True)
    config_json = Column(Text)  # Use JSON if your DB supports it
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow)

    results = relationship("TestResult", back_populates="test_definition")


class TestResult(Base):
    __tablename__ = "test_results"

    id = Column(Integer, primary_key=True, index=True)
    test_definition_id = Column(Integer, ForeignKey("test_definitions.id"))
    run_timestamp = Column(DateTime, default=datetime.utcnow)
    target_system = Column(String)
    result_status = Column(String)
    result_data = Column(Text)  # JSON or text
    comparison_data = Column(Text)  # JSON or text
    notes = Column(Text, nullable=True)

    test_definition = relationship("TestDefinition", back_populates="results")
''',

    "schemas.py": '''from pydantic import BaseModel
from typing import Optional
from datetime import datetime

class TestDefinitionBase(BaseModel):
    name: str
    description: Optional[str] = None
    config_json: str

class TestDefinitionCreate(TestDefinitionBase):
    pass

class TestDefinition(TestDefinitionBase):
    id: int
    created_at: datetime
    updated_at: datetime

    class Config:
        orm_mode = True


class TestResultBase(BaseModel):
    test_definition_id: int
    target_system: str
    result_status: str
    result_data: str
    comparison_data: str
    notes: Optional[str] = None

class TestResultCreate(TestResultBase):
    pass

class TestResult(TestResultBase):
    id: int
    run_timestamp: datetime

    class Config:
        orm_mode = True
''',

    "crud.py": '''from sqlalchemy.orm import Session
from models import TestDefinition, TestResult
from schemas import TestDefinitionCreate, TestResultCreate
from datetime import datetime

def create_test_definition(db: Session, test_def: TestDefinitionCreate):
    db_test_def = TestDefinition(
        name=test_def.name,
        description=test_def.description,
        config_json=test_def.config_json,
        created_at=datetime.utcnow(),
        updated_at=datetime.utcnow()
    )
    db.add(db_test_def)
    db.commit()
    db.refresh(db_test_def)
    return db_test_def

def get_test_definition(db: Session, test_def_id: int):
    return db.query(TestDefinition).filter(TestDefinition.id == test_def_id).first()

def create_test_result(db: Session, test_result: TestResultCreate):
    db_test_result = TestResult(
        test_definition_id=test_result.test_definition_id,
        target_system=test_result.target_system,
        result_status=test_result.result_status,
        result_data=test_result.result_data,
        comparison_data=test_result.comparison_data,
        notes=test_result.notes,
        run_timestamp=datetime.utcnow()
    )
    db.add(db_test_result)
    db.commit()
    db.refresh(db_test_result)
    return db_test_result

def get_test_results_by_definition(db: Session, test_def_id: int):
    return db.query(TestResult).filter(TestResult.test_definition_id == test_def_id).all()
''',

    "main.py": '''from fastapi import FastAPI, Depends, HTTPException
from sqlalchemy.orm import Session
import uvicorn
import os

from models import Base
from database import engine, SessionLocal
import schemas, crud

Base.metadata.create_all(bind=engine)

app = FastAPI()

# Dependency to get DB session
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

@app.post("/test-definitions/", response_model=schemas.TestDefinition)
def create_test_definition(test_def: schemas.TestDefinitionCreate, db: Session = Depends(get_db)):
    db_test_def = crud.create_test_definition(db, test_def)
    return db_test_def

@app.get("/test-definitions/{test_def_id}", response_model=schemas.TestDefinition)
def read_test_definition(test_def_id: int, db: Session = Depends(get_db)):
    db_test_def = crud.get_test_definition(db, test_def_id)
    if db_test_def is None:
        raise HTTPException(status_code=404, detail="Test definition not found")
    return db_test_def

@app.post("/test-results/", response_model=schemas.TestResult)
def create_test_result(test_result: schemas.TestResultCreate, db: Session = Depends(get_db)):
    return crud.create_test_result(db, test_result)

@app.get("/test-definitions/{test_def_id}/results", response_model=list[schemas.TestResult])
def read_test_results(test_def_id: int, db: Session = Depends(get_db)):
    return crud.get_test_results_by_definition(db, test_def_id)

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run(app, host="0.0.0.0", port=port)
''',

    "static/index.html": '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Test System Interface</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        h1 { color: #333; }
        label { display: block; margin-top: 10px; }
        input, textarea, select { width: 100%; padding: 8px; margin-top: 4px; box-sizing: border-box; }
        button { margin-top: 15px; padding: 10px 15px; background-color: #007bff; color: white; border: none; cursor: pointer; }
        button:hover { background-color: #0056b3; }
        .container { max-width: 600px; margin: auto; }
        .result { margin-top: 20px; padding: 10px; border: 1px solid #ccc; background: #f9f9f9; white-space: pre-wrap; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Test System Interface</h1>

        <h2>Create Test Definition</h2>
        <form id="testDefForm">
            <label for="name">Name:</label>
            <input type="text" id="name" name="name" required />

            <label for="description">Description:</label>
            <textarea id="description" name="description"></textarea>

            <label for="config_json">Config JSON:</label>
            <textarea id="config_json" name="config_json" rows="6" required></textarea>

            <button type="submit">Create Test Definition</button>
        </form>

        <div id="testDefResult" class="result"></div>

        <h2>Get Test Definition</h2>
        <form id="getTestDefForm">
            <label for="testDefId">Test Definition ID:</label>
            <input type="number" id="testDefId" name="testDefId" required />
            <button type="submit">Get Test Definition</button>
        </form>

        <div id="getTestDefResult" class="result"></div>

        <h2>Create Test Result</h2>
        <form id="testResultForm">
            <label for="test_definition_id">Test Definition ID:</label>
            <input type="number" id="test_definition_id" name="test_definition_id" required />

            <label for="target_system">Target System:</label>
            <input type="text" id="target_system" name="target_system" required />

            <label for="result_status">Result Status:</label>
            <input type="text" id="result_status" name="result_status" required />

            <label for="result_data">Result Data (JSON):</label>
            <textarea id="result_data" name="result_data" rows="4" required></textarea>

            <label for="comparison_data">Comparison Data (JSON):</label>
            <textarea id="comparison_data" name="comparison_data" rows="4" required></textarea>

            <label for="notes">Notes:</label>
            <textarea id="notes" name="notes"></textarea>

            <button type="submit">Create Test Result</button>
        </form>

        <div id="testResultResult" class="result"></div>

        <h2>Get Test Results by Test Definition</h2>
        <form id="getTestResultsForm">
            <label for="testDefIdResults">Test Definition ID:</label>
            <input type="number" id="testDefIdResults" name="testDefIdResults" required />
            <button type="submit">Get Test Results</button>
        </form>

        <div id="getTestResultsResult" class="result"></div>
    </div>

    <script>
        const apiBase = '';

        document.getElementById('testDefForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const data = {
                name: e.target.name.value,
                description: e.target.description.value,
                config_json: e.target.config_json.value
            };
            const res = await fetch(apiBase + '/test-definitions/', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });
            const json = await res.json();
            document.getElementById('testDefResult').textContent = JSON.stringify(json, null, 2);
        });

        document.getElementById('getTestDefForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const id = e.target.testDefId.value;
            const res = await fetch(apiBase + `/test-definitions/${id}`);
            const json = await res.json();
            document.getElementById('getTestDefResult').textContent = JSON.stringify(json, null, 2);
        });

        document.getElementById('testResultForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const data = {
                test_definition_id: parseInt(e.target.test_definition_id.value),
                target_system: e.target.target_system.value,
                result_status: e.target.result_status.value,
                result_data: e.target.result_data.value,
                comparison_data: e.target.comparison_data.value,
                notes: e.target.notes.value
            };
            const res = await fetch(apiBase + '/test-results/', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });
            const json = await res.json();
            document.getElementById('testResultResult').textContent = JSON.stringify(json, null, 2);
        });

        document.getElementById('getTestResultsForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const id = e.target.testDefIdResults.value;
            const res = await fetch(apiBase + `/test-definitions/${id}/results`);
            const json = await res.json();
            document.getElementById('getTestResultsResult').textContent = JSON.stringify(json, null, 2);
        });
    </script>
</body>
</html>
''',

    "requirements.txt": '''fastapi
uvicorn[standard]
sqlalchemy
pydantic
'''
}

def create_project():
    # Create directories
    if not os.path.exists("static"):
        os.makedirs("static")

    # Write files
    for filename, content in project_files.items():
        with open(filename, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"Created {filename}")

if __name__ == "__main__":
    create_project()
    print("Project files created successfully. You can now install dependencies with 'pip install -r requirements.txt' and run the app with 'python main.py'.")