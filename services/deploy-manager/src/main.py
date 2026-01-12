from pathlib import Path
from .logger import get_logger
from .manifest.manifest_fetcher import ManifestFetcher
from .manifest.parser import ManifestParser
from .artifact.materializer import TaskArtifactMaterializer
from .docker.builder import DockerImageBuilder
from .exceptions import DeployManagerError
import sys

logger = get_logger(__name__)

def main():
    repo_url = "https://github.com/GeoDimi99/RT-Task-Spec.git"
    mission_path = Path("/tmp/rt-mission-spec")

    task_service_path = Path("/app/task-service")
    task_service_include = task_service_path / "include"

    fetcher = ManifestFetcher(repo_url, mission_path)

    try:
        fetcher.fetch()

        parser = ManifestParser(
            str(mission_path / "task_manifest.yaml")
        )
        schedule = parser.parse()

        materializer = TaskArtifactMaterializer(
            mission_repo_path=mission_path,
            task_service_include_path=task_service_include,
        )

        docker_builder = DockerImageBuilder()

        for task in schedule.tasks:
            materializer.materialize_task(task)

            image_tag = f"task-service:{task.name}-{schedule.version}"
            docker_builder.build_task_service_image(
                build_context=task_service_path,
                image_tag=image_tag,
            )

    except DeployManagerError as e:
        logger.error(f"Deploy Manager failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
