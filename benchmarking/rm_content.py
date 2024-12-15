import os
import shutil
import sys

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Unexpected number of arguments") 

    for root, dirs, files in os.walk(sys.argv[1]):
        for f in files:
            os.unlink(os.path.join(root, f))
        for d in dirs:
            shutil.rmtree(os.path.join(root, d))