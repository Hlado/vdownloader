import subprocess
import sys

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Unexpected number of arguments") 

    fileUrl = sys.argv[1]

    processes = []
    for i in range(10):
        start = 10+i*20
        finish = start + 10
        
        #shell = True breaks fileUrl part, escaping doesn't help
        p = subprocess.Popen(
            ["ffmpeg",
                "-hide_banner",
                "-loglevel", "error",
                "-nostats",
                "-ss", str(start),
                "-to", str(finish),
                "-i", fileUrl,
                "-r", "1",
                f"s{i}f%01d.bmp"],
            stderr = subprocess.PIPE)
        processes.append(p)
        
    exit_code = 0
    for p in processes:
        p.wait()
        if not p.returncode == 0:
            print(p.stderr)
            exit_code = 1
            
    sys.exit(exit_code) 