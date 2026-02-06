import argparse
import os
import sys

from dslparser.parser import DSL_Parser
from dslparser.exceptions import ParserError




def main():

    # Init phase: take input arguments from the user or set the default parameters
    parser = argparse.ArgumentParser(
        description="Image Builder CLI: Compiles microservice images based on a Model-Driven manifest."
    )

    parser.add_argument(
        "-f", "--file-manifest", 
        default="manifest.yaml",
        metavar="FILE",
        help="Path to the manifest file containing image aliases and task definitions (default: manifest.yaml)"
    )

    parser.add_argument(
        "-c", "--context", 
        default=".", 
        metavar="PATH",
        help="Directory context containing the source code for each microservice (default: current directory)"
    )

    args = parser.parse_args()

    file_manifest = args.file_manifest
    context = args.context

    # Parsing phase : read the manifest file
    try:
        dslparser = DSL_Parser(args.file_manifest)
        data = dslparser.parse()
        
        # Check if the 'src' directories exist relative to the context
        for img in data.get('images', []):
            src_path = os.path.join(args.context, img['src'])
            if not os.path.isdir(src_path):
                # We raise this here because main knows about the context path
                raise ImageBuilderError(f"Source path '{src_path}' for alias '{img['alias']}' does not exist.")
        
        images = data.get('images', [])
        print("Manifest validated. Ready to build.")

    except ParserError as e:
        # Professional CLI error reporting
        print(f"Build Error: {e}", file=sys.stderr)
        sys.exit(1)

    except Exception as e:
        # Catch-all for unexpected crashes
        print(f"An unexpected internal error occurred: {e}", file=sys.stderr)
        sys.exit(1)
    

    # Building phase :
    print(images)


if __name__ == "__main__":
    main()