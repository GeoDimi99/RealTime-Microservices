import docker
from pathlib import Path
from ..logger import get_logger
from ..exceptions import DeployManagerError

logger = get_logger(__name__)

class DockerImageBuilder:
    """
    Builds Docker images for task services.
    """

    def __init__(self):
        self.client = docker.from_env()

    def build_task_service_image(
        self,
        build_context: Path,
        image_tag: str,
        task_queue_prefix: str = "/task"
    ) -> None:
        try:
            logger.info(
                f"Building Docker image '{image_tag}' "
                f"from {build_context}"
            )
            image, logs = self.client.images.build(
                path=str(build_context),
                tag=image_tag,
                rm=True,
                buildargs={"TASK_QUEUE_PREFIX": task_queue_prefix}
            )
            logger.info(f"Successfully built image '{image_tag}'")
        except docker.errors.BuildError as e:
            raise DeployManagerError(f"Docker build failed: {e}")
        except Exception as e:
            raise DeployManagerError(f"Docker error: {e}")
