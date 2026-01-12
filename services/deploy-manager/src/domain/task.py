# src/domain/task.py
from dataclasses import dataclass, field
from typing import List

@dataclass
class Task:
    name: str
    policy: str
    priority: int
    depends_on: List[str] = field(default_factory=list)
    inputs: List[str] = field(default_factory=list)
    outputs: List[str] = field(default_factory=list)
