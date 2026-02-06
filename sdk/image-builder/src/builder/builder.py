import os
from string import Template

class Builder:
    BASE_IMAGE = "geodimi99/realtime-microservices:task-wrapper"
    TEMPLATE_PATH = os.path.join(os.path.dirname(__file__), "templates/Dockerfile.j2")

    def __init__(self, context_path):
        self.context_path = context_path

    def _get_template(self):
        with open(self.TEMPLATE_PATH, "r") as f:
            return Template(f.read())

    def generate_dockerfile(self, service_name, source_dir):
        template = self._get_template()
        
        # Map Python variables to Template placeholders
        content = template.substitute(
            base_image=self.BASE_IMAGE,
            source_dir=source_dir
        )

        df_path = os.path.join(self.context_path, f"Dockerfile.{service_name}")
        with open(df_path, "w") as f:
            f.write(content)
            
        return df_path