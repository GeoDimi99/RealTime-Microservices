from pathlib import Path
from .logger import get_logger
from .manifest.manifest_fetcher import ManifestFetcher
from .manifest.parser import ManifestParser
from .deploy.materializer import TaskArtifactMaterializer
from .deploy.builder import DockerImageBuilder
from .deploy.runner import DockerContainerRunner
from .database.redis_loader import RedisLoader  # NEW
from .exceptions import DeployManagerError
import sys

logger = get_logger(__name__)

def main():
    repo_url = "https://github.com/GeoDimi99/RT-Task-Spec.git"
    mission_path = Path("/tmp/rt-mission-spec")

    task_service_path = Path("/app/task-service")
    task_service_include = task_service_path / "include"

    fetcher = ManifestFetcher(repo_url, mission_path)
    docker_builder = DockerImageBuilder()
    docker_runner = DockerContainerRunner()
    redis_loader = RedisLoader(host="redis", port=6379)  # NEW

    try:
        # Fetch mission repository
        fetcher.fetch()

        # Parse manifest
        parser = ManifestParser(str(mission_path / "task_manifest.yaml"))
        schedule = parser.parse()

        # Materialize task entries
        materializer = TaskArtifactMaterializer(
            mission_repo_path=mission_path,
            task_service_include_path=task_service_include,
        )

        # Build and run containers for each task
        for task in schedule.tasks:
            logger.info(f"Processing task '{task.name}'")

            # Materialize task header
            materializer.materialize_task(task)

            # Build Docker image
            image_tag = f"task-service:{task.name}-{schedule.version}"
            docker_builder.build_task_service_image(
                build_context=task_service_path,
                image_tag=image_tag,
                task_queue_prefix="/" + task.name
            )

            # Run container
            container_name = f"task-service-{task.name}"
            docker_runner.run_task_service(
                image_tag=image_tag,
                container_name=container_name,
            )
        
        # Load schedule and tasks into Redis
        redis_loader.load_schedule(schedule)
        redis_loader.debug_print()  # REMOVE AFTER DEBUGGING
        logger.info("Schedule data loaded into Redis successfully.")
        

    except DeployManagerError as e:
        logger.error(f"Deploy Manager failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
