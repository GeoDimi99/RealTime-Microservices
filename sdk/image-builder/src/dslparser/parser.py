import yaml

class DSL_Parser:

    def __init__(self, file_path):
        self.file_path = file_path

    def parse(self):
        with open(self.file_path, 'r') as file:
            data = yaml.safe_load(file)
        
        # Return the task list from the YAML strucure provided
        return data