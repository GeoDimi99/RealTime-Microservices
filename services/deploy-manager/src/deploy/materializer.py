from pathlib import Path
import shutil
from ..logger import get_logger
from ..exceptions import DeployManagerError
from ..domain.task import Task

logger = get_logger(__name__)

class TaskArtifactMaterializer:
    """
    Materializes task-specific artifacts into the task-service build context.
    """

    def __init__(
        self,
        mission_repo_path: Path,
        task_service_include_path: Path,
    ):
        self.mission_repo_path = mission_repo_path
        self.task_service_include_path = task_service_include_path

    def materialize_task(self, task: Task) -> None:
        """
        Copy task_entry.h for a given task into task-service/include/
        """
        source_header = (
            self.mission_repo_path
            / task.name
            / "task_entry.h"
        )

        if not source_header.exists():
            raise DeployManagerError(
                f"task_entry.h not found for task '{task.name}' "
                f"at {source_header}"
            )

        destination = self.task_service_include_path / "task_entry.h"

        try:
            shutil.copyfile(source_header, destination)
            logger.info(
                f"Injected task '{task.name}' entry header into task-service"
            )
        except Exception as e:
            raise DeployManagerError(
                f"Failed to materialize task '{task.name}': {e}"
            )
