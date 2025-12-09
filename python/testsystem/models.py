from sqlalchemy import Column, Integer, String, Text, ForeignKey, DateTime
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
