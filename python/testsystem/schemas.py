from pydantic import BaseModel
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
