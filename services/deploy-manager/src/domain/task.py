# domain/task.py
from dataclasses import dataclass, field
from typing import Dict, List, Any

@dataclass
class Task:
    name: str
    policy: str
    priority: int
    depends_on: List[str] = field(default_factory=list)
    inputs: Dict[str, Any] = field(default_factory=dict)
    outputs: Dict[str, Any] = field(default_factory=dict)
