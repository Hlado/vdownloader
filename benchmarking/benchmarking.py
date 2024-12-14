import os
import subprocess
import sys

if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit("Unexpected number of arguments") 

    executable = sys.argv[1]
    url = sys.argv[2]
    format = sys.argv[3]
    times = sys.argv[4]        
    
    fileUrl = subprocess.check_output(["yt-dlp", "--quiet", "-f", format, "--get-url", url], text = True).strip()   
    ffmpegScript = os.path.join(os.path.dirname(sys.argv[0]), "invoke_ffmpeg.py")
    
    subprocess.run(
        ["hyperfine",
            "--show-output",
            "-p", "del /q .",
            "-c", "del /q .",
            "-r", times,
            f"\"\"{executable}\" \"{fileUrl}\" 10s-20s:9 30s-40s:9 50s-60s:9 70s-80s:9 90s-100s:9 110s-120s:9 130s-140s:9 150s-160s:9 170s-180s:9 190s-200s:9\"",
            f"\"python \"{ffmpegScript}\" \"{fileUrl}\"\""],
        check = True)