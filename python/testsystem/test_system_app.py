# test_system_app.py

import json
import os
from datetime import datetime
from typing import List, Optional, Any

from fastapi import FastAPI, Depends, HTTPException, BackgroundTasks
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from sqlalchemy import Column, Integer, String, Text, DateTime, create_engine
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, Session
from pydantic import BaseModel

# -----------------------------
# Pydantic Schemas
# -----------------------------

class TestAction(BaseModel):
    action: str
    desc: Optional[str] = None
    data: Optional[List[Any]] = []

class TestDefinitionBase(BaseModel):
    test: str
    name: Optional[str] = None
    desc: Optional[str] = None
    items: List[TestAction]

class TestDefinitionCreate(TestDefinitionBase):
    pass

class TestDefinition(TestDefinitionBase):
    id: int

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

# -----------------------------
# Database Setup
# -----------------------------

SQLALCHEMY_DATABASE_URL = "sqlite:///./test_system.db"
engine = create_engine(SQLALCHEMY_DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()

# -----------------------------
# SQLAlchemy Models
# -----------------------------

class TestDefinitionDB(Base):
    __tablename__ = "test_definitions"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String, unique=True, index=True)
    description = Column(Text, nullable=True)
    config_json = Column(Text)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow)

class TestResultDB(Base):
    __tablename__ = "test_results"

    id = Column(Integer, primary_key=True, index=True)
    test_definition_id = Column(Integer)
    run_timestamp = Column(DateTime, default=datetime.utcnow)
    target_system = Column(String)
    result_status = Column(String)
    result_data = Column(Text)
    comparison_data = Column(Text)
    notes = Column(Text, nullable=True)

Base.metadata.create_all(bind=engine)

# -----------------------------
# CRUD Functions
# -----------------------------

def create_test_definition(db: Session, test_def: TestDefinitionCreate):
    db_test_def = TestDefinitionDB(
        name=test_def.test,
        description=test_def.desc,
        config_json=json.dumps(test_def.dict()),
        created_at=datetime.utcnow(),
        updated_at=datetime.utcnow()
    )
    db.add(db_test_def)
    db.commit()
    db.refresh(db_test_def)
    return db_test_def

def get_test_definition(db: Session, test_def_id: int):
    return db.query(TestDefinitionDB).filter(TestDefinitionDB.id == test_def_id).first()

def create_test_result(db: Session, test_result: TestResultCreate):
    db_test_result = TestResultDB(
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
    return db.query(TestResultDB).filter(TestResultDB.test_definition_id == test_def_id).all()

# -----------------------------
# FastAPI App
# -----------------------------

app = FastAPI()

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

TEST_DEFINITIONS_JSON = "test_definitions.json"

def load_test_definitions_from_json(db):
    try:
        with open(TEST_DEFINITIONS_JSON, "r") as f:
            test_defs = json.load(f)
        for td in test_defs:
            name = td.get("test") or td.get("name")
            existing = db.query(TestDefinitionDB).filter(TestDefinitionDB.name == name).first()
            if existing:
                existing.description = td.get("desc", "")
                existing.config_json = json.dumps(td)
                existing.updated_at = datetime.utcnow()
            else:
                new_td = TestDefinitionDB(
                    name=name,
                    description=td.get("desc", ""),
                    config_json=json.dumps(td),
                    created_at=datetime.utcnow(),
                    updated_at=datetime.utcnow()
                )
                db.add(new_td)
        db.commit()
    except Exception as e:
        print(f"Error loading test definitions from JSON: {e}")

@app.on_event("startup")
def startup_event():
    db = SessionLocal()
    load_test_definitions_from_json(db)
    db.close()

# -----------------------------
# API Endpoints
# -----------------------------

@app.get("/")
def read_index():
    return FileResponse("static/index.html")

@app.get("/test-definitions/", response_model=List[TestDefinition])
def list_test_definitions(db: Session = Depends(get_db)):
    defs = db.query(TestDefinitionDB).all()
    results = []
    for d in defs:
        config = json.loads(d.config_json) if d.config_json else {}
        obj = {
            "id": d.id,
            "test": config.get("test") or config.get("name") or d.name,
            "name": config.get("name") or d.name,
            "desc": config.get("desc") or d.description,
            "items": config.get("items", [])
        }
        results.append(obj)
    return results

@app.get("/test-definitions/{test_def_id}", response_model=TestDefinition)
def read_test_definition(test_def_id: int, db: Session = Depends(get_db)):
    db_test_def = get_test_definition(db, test_def_id)
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")
    config = json.loads(db_test_def.config_json)
    return {
        "id": db_test_def.id,
        "test": config.get("test") or config.get("name") or db_test_def.name,
        "name": config.get("name") or db_test_def.name,
        "desc": config.get("desc") or db_test_def.description,
        "items": config.get("items", [])
    }

@app.post("/test-definitions/", response_model=TestDefinition)
def create_test_definition_api(test_def: TestDefinitionCreate, db: Session = Depends(get_db)):
    return create_test_definition(db, test_def)

@app.put("/test-definitions/{test_def_id}", response_model=TestDefinition)
def update_test_definition(test_def_id: int, test_def: TestDefinitionCreate, db: Session = Depends(get_db)):
    db_test_def = db.query(TestDefinitionDB).filter(TestDefinitionDB.id == test_def_id).first()
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")
    db_test_def.name = test_def.test
    db_test_def.description = test_def.desc
    db_test_def.config_json = json.dumps(test_def.dict())
    db_test_def.updated_at = datetime.utcnow()
    db.commit()
    db.refresh(db_test_def)
    return {
        "id": db_test_def.id,
        "test": test_def.test,
        "name": test_def.name,
        "desc": test_def.desc,
        "items": test_def.items
    }

@app.delete("/test-definitions/{test_def_id}")
def delete_test_definition(test_def_id: int, db: Session = Depends(get_db)):
    db_test_def = db.query(TestDefinitionDB).filter(TestDefinitionDB.id == test_def_id).first()
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")
    db.delete(db_test_def)
    db.commit()
    return {"detail": "Test definition deleted"}

@app.post("/test-definitions/{test_def_id}/copy", response_model=TestDefinition)
def copy_test_definition(test_def_id: int, db: Session = Depends(get_db)):
    db_test_def = db.query(TestDefinitionDB).filter(TestDefinitionDB.id == test_def_id).first()
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")
    config = json.loads(db_test_def.config_json)
    new_name = config["test"] + "_copy"
    count = 1
    while db.query(TestDefinitionDB).filter(TestDefinitionDB.name == new_name).first():
        new_name = f"{config['test']}_copy{count}"
        count += 1
    config["test"] = new_name
    new_td = TestDefinitionDB(
        name=new_name,
        description=config.get("desc", ""),
        config_json=json.dumps(config),
        created_at=datetime.utcnow(),
        updated_at=datetime.utcnow()
    )
    db.add(new_td)
    db.commit()
    db.refresh(new_td)
    return {
        "id": new_td.id,
        "test": config["test"],
        "name": config.get("name"),
        "desc": config.get("desc"),
        "items": config.get("items", [])
    }

@app.post("/test-definitions/{test_def_id}/run")
def run_test(test_def_id: int, background_tasks: BackgroundTasks, db: Session = Depends(get_db)):
    def execute_test(test_id):
        print(f"Running test {test_id}... (stub)")

    db_test_def = db.query(TestDefinitionDB).filter(TestDefinitionDB.id == test_def_id).first()
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")

    background_tasks.add_task(execute_test, test_def_id)
    return {"detail": f"Test {test_def_id} started"}

@app.post("/test-results/", response_model=TestResult)
def create_test_result_api(test_result: TestResultCreate, db: Session = Depends(get_db)):
    return create_test_result(db, test_result)

@app.get("/test-definitions/{test_def_id}/results", response_model=List[TestResult])
def read_test_results(test_def_id: int, db: Session = Depends(get_db)):
    return get_test_results_by_definition(db, test_def_id)

# -----------------------------
# Static Files
# -----------------------------

app.mount("/static", StaticFiles(directory="static"), name="static")

if __name__ == "__main__":
    import uvicorn
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run(app, host="0.0.0.0", port=port)

