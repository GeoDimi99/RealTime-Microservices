import argparse
import os

from dslparser.parser import DSL_Parser


def main():
    # A professional description clearly stating the script's purpose
    parser = argparse.ArgumentParser(
        description="Image Builder CLI: Compiles microservice images based on a Model-Driven manifest."
    )
    
    # -f: Pointing to the specific YAML configuration
    parser.add_argument(
        "-f", "--file-manifest", 
        default="manifest.yaml",
        metavar="FILE",
        help="Path to the YAML manifest file containing image aliases and task definitions (default: manifest.yaml)"
    )
    
    # -c: Defining the working directory where the source folders (sum, subtract) live
    parser.add_argument(
        "-c", "--context", 
        default=".", 
        metavar="PATH",
        help="Directory context containing the source code for each microservice (default: current directory)"
    )

    args = parser.parse_args()

    # Accessing the arguments
    print(f"Loading manifest from: {args.file_manifest}")
    print(f"Building context: {args.context}")

    file_manifest = args.file_manifest
    context = args.context

    dslparser = DSL_Parser(file_manifest)
    data = dslparser.parse()
    print(data)





if __name__ == "__main__":
    main()