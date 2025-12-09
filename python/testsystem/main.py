from fastapi import FastAPI, Depends, HTTPException, BackgroundTasks
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse

from sqlalchemy.orm import Session
import uvicorn
import os
import json
from datetime import datetime

from models import Base, TestDefinition
from database import engine, SessionLocal
import schemas, crud

TEST_DEFINITIONS_JSON = "test_definitions.json"

Base.metadata.create_all(bind=engine)

app = FastAPI()

# Mount the static directory
app.mount("/static", StaticFiles(directory="static"), name="static")

# Serve index.html at root
@app.get("/")
def read_index():
    return FileResponse("static/index.html")


# Dependency to get DB session
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

def load_test_definitions_from_json(db):
    try:
        with open(TEST_DEFINITIONS_JSON, "r") as f:
            test_defs = json.load(f)
        for td in test_defs:
            # Check if exists by name
            existing = db.query(TestDefinition).filter(TestDefinition.name == td["name"]).first()
            if existing:
                # Update existing
                existing.description = td.get("description", "")
                existing.config_json = json.dumps(td.get("config_json", {})) if isinstance(td.get("config_json"), dict) else td.get("config_json", "")
                existing.updated_at = datetime.utcnow()
            else:
                # Create new
                new_td = TestDefinition(
                    name=td["name"],
                    description=td.get("description", ""),
                    config_json=json.dumps(td.get("config_json", {})) if isinstance(td.get("config_json"), dict) else td.get("config_json", ""),
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

@app.get("/test-definitions/", response_model=list[schemas.TestDefinition])
def list_test_definitions(db: Session = Depends(get_db)):
    return db.query(TestDefinition).all()

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

@app.put("/test-definitions/{test_def_id}", response_model=schemas.TestDefinition)
def update_test_definition(test_def_id: int, test_def: schemas.TestDefinitionCreate, db: Session = Depends(get_db)):
    db_test_def = db.query(TestDefinition).filter(TestDefinition.id == test_def_id).first()
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")
    db_test_def.name = test_def.name
    db_test_def.description = test_def.description
    db_test_def.config_json = test_def.config_json
    db_test_def.updated_at = datetime.utcnow()
    db.commit()
    db.refresh(db_test_def)
    return db_test_def

@app.delete("/test-definitions/{test_def_id}")
def delete_test_definition(test_def_id: int, db: Session = Depends(get_db)):
    db_test_def = db.query(TestDefinition).filter(TestDefinition.id == test_def_id).first()
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")
    db.delete(db_test_def)
    db.commit()
    return {"detail": "Test definition deleted"}

@app.post("/test-definitions/{test_def_id}/copy", response_model=schemas.TestDefinition)
def copy_test_definition(test_def_id: int, db: Session = Depends(get_db)):
    db_test_def = db.query(TestDefinition).filter(TestDefinition.id == test_def_id).first()
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")
    new_name = db_test_def.name + "_copy"
    # Ensure unique name
    count = 1
    while db.query(TestDefinition).filter(TestDefinition.name == new_name).first():
        new_name = f"{db_test_def.name}_copy{count}"
        count += 1
    new_td = TestDefinition(
        name=new_name,
        description=db_test_def.description,
        config_json=db_test_def.config_json,
        created_at=datetime.utcnow(),
        updated_at=datetime.utcnow()
    )
    db.add(new_td)
    db.commit()
    db.refresh(new_td)
    return new_td

@app.post("/test-definitions/{test_def_id}/run")
def run_test(test_def_id: int, background_tasks: BackgroundTasks, db: Session = Depends(get_db)):
    # Stub: Add your test execution logic here
    def execute_test(test_id):
        print(f"Running test {test_id}... (stub)")

    db_test_def = db.query(TestDefinition).filter(TestDefinition.id == test_def_id).first()
    if not db_test_def:
        raise HTTPException(status_code=404, detail="Test definition not found")

    background_tasks.add_task(execute_test, test_def_id)
    return {"detail": f"Test {test_def_id} started"}

@app.post("/test-results/", response_model=schemas.TestResult)
def create_test_result(test_result: schemas.TestResultCreate, db: Session = Depends(get_db)):
    return crud.create_test_result(db, test_result)

@app.get("/test-definitions/{test_def_id}/results", response_model=list[schemas.TestResult])
def read_test_results(test_def_id: int, db: Session = Depends(get_db)):
    return crud.get_test_results_by_definition(db, test_def_id)

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 8000))
    uvicorn.run(app, host="0.0.0.0", port=port)

