import subprocess
import sys

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Unexpected number of arguments") 

    fileUrl = sys.argv[1]

    for i in range(10):
        start = 10+i*20
        finish = start + 10
        
        #shell = True breaks fileUrl part, escaping doesn't help
        result = subprocess.run(
            ["ffmpeg",
                "-hide_banner",
                "-loglevel", "error",
                "-nostats",
                "-ss", str(start),
                "-to", str(finish),
                "-i", fileUrl,
                "-r", "1",
                f"s{i}f%01d.bmp"],
            check = True)