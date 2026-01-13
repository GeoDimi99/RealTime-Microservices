# src/deploy/redis_loader.py
import redis
from ..logger import get_logger
from ..domain.schedule import Schedule

logger = get_logger(__name__)

class RedisLoader:
    """
    Loads schedule/task data into Redis for debugging purposes.
    """

    def __init__(self, host="localhost", port=6379, db=0):
        self.client = redis.Redis(host=host, port=port, db=db, decode_responses=True)

    def load_schedule(self, schedule: Schedule):
        """
        Push schedule and tasks into Redis.
        """
        self.client.hset("schedule", mapping={
            "name": schedule.name,
            "version": schedule.version,
            "description": schedule.description,
            "length": str(len(schedule.tasks))
        })

        for i, task in enumerate(schedule.tasks, start=1):
            task_key = f"scheduletask:{i}"
            self.client.hset(task_key, mapping={
                "name": task.name,
                "policy": task.policy,
                "priority": str(task.priority),
                "depends_on": ",".join(task.depends_on),
                "inputs": ",".join(task.inputs),
                "outputs": ",".join(task.outputs)
            })

        logger.info(f"Loaded {len(schedule.tasks)} tasks into Redis.")

    def debug_print(self):
        """
        Read back all schedule/task data and print for debugging.
        """
        schedule_data = self.client.hgetall("schedule")
        print("=== SCHEDULE ===")
        for k, v in schedule_data.items():
            print(f"{k}: {v}")

        print("\n=== TASKS ===")
        length = int(schedule_data.get("length", 0))
        for i in range(1, length + 1):
            task_key = f"scheduletask:{i}"
            task_data = self.client.hgetall(task_key)
            print(f"{task_key}: {task_data}")
