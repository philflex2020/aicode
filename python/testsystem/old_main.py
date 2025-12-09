from fastapi import FastAPI, Depends, HTTPException
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
