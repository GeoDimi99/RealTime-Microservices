# src/main.py
from pathlib import Path
from .logger import get_logger
from .manifest.manifest_fetcher import ManifestFetcher
from .manifest.parser import ManifestParser
from .exceptions import DeployManagerError
import sys

logger = get_logger(__name__)

def main():
    # Example: replace with your Git repository URL
    repo_url = "https://github.com/GeoDimi99/RT-Task-Spec.git"
    local_path = Path("/tmp/rt-mission-spec")

    fetcher = ManifestFetcher(repo_url, local_path)

    try:
        fetcher.fetch()
        manifest_file = local_path / "task_manifest.yaml"
        if manifest_file.exists():
            parser = ManifestParser(str(manifest_file))
            schedule = parser.parse()

            # Example: print tasks
            for task in schedule.tasks:
                logger.info(f"Task: {task.id}, policy={task.policy}, priority={task.priority}")
        else:
            logger.warning("Manifest file does not exist in the repository.")
    except DeployManagerError as e:
        logger.error(f"Failed to fetch manifest: {e}")
        sys.exit(1)
    


if __name__ == "__main__":
    main()
