from pathlib import Path
from .logger import get_logger
from .manifest.parser import ManifestParser
from .deploy.runner import DockerContainerRunner
from .database.redis_loader import RedisLoader
from .exceptions import DeployManagerError
import sys
import socket
import time

logger = get_logger(__name__)

def wait_for_grpc_ready(host, port, timeout=30, retry_interval=0.5):
    """
    Wait for gRPC server to be ready by attempting to connect to the port.
    """
    logger.info(f"Waiting for gRPC server at {host}:{port} to be ready...")
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            result = sock.connect_ex((host, port))
            sock.close()
            
            if result == 0:
                logger.info(f"✅ gRPC server at {host}:{port} is ready!")
                return True
        except Exception as e:
            pass
        
        time.sleep(retry_interval)
    
    logger.warning(f"⚠️ gRPC server at {host}:{port} not ready after {timeout}s")
    return False

def main():
    # Use local manifest instead of cloning from GitHub
    manifest_path = Path("/app/task/manifest.yaml")

    task_service_path = Path("/app/task-wrapper")
    task_service_include = task_service_path / "include"

    docker_runner = DockerContainerRunner()
    # With host networking, use localhost instead of service name
    redis_loader = RedisLoader(host="localhost", port=6379)

    try:
        # Parse manifest
        parser = ManifestParser(str(manifest_path))
        schedule = parser.parse()

        # Extract unique images from all tasks
        # Multiple tasks can use the same image (container), each execution creates a new thread
        unique_images = set()
        for task in schedule.tasks:
            unique_images.add(task.name)
        
        logger.info(f"Found {len(unique_images)} unique task image(s) for {len(schedule.tasks)} task(s)")

        # Run ONE container per unique image
        for image_name in unique_images:
            logger.info(f"Deploying container for image '{image_name}'")
            
            container_name = f"task-service-{image_name}"
            docker_runner.run_task_service(
                image_tag=image_name,
                container_name=container_name,
            )
            
            # Wait for gRPC server to be ready
            # With host networking, use localhost instead of container name
            if not wait_for_grpc_ready("localhost", 50051, timeout=30):
                raise DeployManagerError(f"gRPC server for '{container_name}' failed to become ready")
        
        # Load schedule and tasks into Redis
        # This loads ALL tasks, even if they share the same image/container
        redis_loader.load_schedule(schedule)
        redis_loader.debug_print()  # REMOVE AFTER DEBUGGING
        logger.info("Schedule data loaded into Redis successfully.")
        

    except DeployManagerError as e:
        logger.error(f"Deploy Manager failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
