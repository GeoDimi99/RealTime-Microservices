# src/manifest/parser.py
from typing import List
import yaml
from ..exceptions import DeployManagerError
from ..logger import get_logger
from ..domain.task import Task
from ..domain.schedule import Schedule

logger = get_logger(__name__)

class ManifestParser:
    # Only fifo and rr for now
    VALID_POLICIES = {"fifo", "rr"}

    def __init__(self, manifest_path: str):
        self.manifest_path = manifest_path

    def parse(self) -> Schedule:
        """
        Parse a YAML manifest file and return a Schedule object composed of Task objects.
        """
        try:
            with open(self.manifest_path, "r") as f:
                data = yaml.safe_load(f)
        except FileNotFoundError:
            raise DeployManagerError(f"Manifest file not found: {self.manifest_path}")
        except yaml.YAMLError as e:
            raise DeployManagerError(f"Error parsing YAML manifest: {e}")

        # Validate top-level fields
        if "schedule" not in data:
            raise DeployManagerError("Manifest missing required field: 'schedule'")

        schedule_data = data["schedule"]
        tasks_data = schedule_data.get("tasks", [])

        # Parse tasks into domain Task objects
        tasks: List[Task] = []
        for t in tasks_data:
            try:
                task = Task(
                    name=t["name"],
                    policy=t["policy"].lower(),
                    priority=int(t["priority"]),
                    depends_on=t.get("depends_on", []),
                    inputs=t.get("inputs", []),
                    outputs=t.get("outputs", []),
                )
            except KeyError as e:
                raise DeployManagerError(f"Task missing required field: {e}")

            # Validate policy
            if task.policy not in self.VALID_POLICIES:
                raise DeployManagerError(f"Invalid policy '{task.policy}' in task {task.id}")

            tasks.append(task)

        # Build Schedule domain object
        schedule = Schedule(
            name=schedule_data.get("name", "unnamed"),
            version=schedule_data.get("version", "0.0.0"),
            description=schedule_data.get("description", ""),
            tasks=tasks
        )

        logger.info(f"Parsed schedule '{schedule.name}' with {len(tasks)} tasks.")
        return schedule
