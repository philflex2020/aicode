from sqlalchemy.orm import Session
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
