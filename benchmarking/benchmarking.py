import os
import pathlib
import re
import subprocess
import urllib.parse

def make_args(args_str):
    clean_str = re.sub('\s+', ' ', args_str).strip()
    if clean_str == "":
        return []
    return re.sub('\s+', ' ', args_str).strip().split(' ');
    
def vdownloader_command(path, url, options = []):
    return \
        f"\"{path}\" " + \
        " ".join(_escape(options)) + \
        f" \"{url}\" 10s-20s:9 30s-40s:9 50s-60s:9 70s-80s:9 90s-100s:9 110s-120s:9 130s-140s:9 150s-160s:9 170s-180s:9 190s-200s:9"

def path_to_script(name):
    return pathlib.Path(__file__).parent.resolve().joinpath(name)
    
def video_file_url(link, format):
    components = urllib.parse.urlparse(link)
    if components.hostname != None and \
           (components.hostname.casefold() == "youtube.com" or \
            components.hostname.casefold() == "www.youtube.com"):
        return subprocess.check_output(["yt-dlp", "--quiet", "-f", f"{format}", "--get-url", f"{link}"], text = True).strip()
    return link
    
def run_benchmark(commands, times):
    rmScript = path_to_script("rm_content.py")
    
    subprocess.run(
        ["hyperfine",
            "--show-output",
            "-p", _escape_on_windows(f"python \"{rmScript}\" ."),
            "-c", _escape_on_windows(f"python \"{rmScript}\" ."),
            "-r", str(times),
            *_escape_on_windows(commands)],
        check = True)
        
def _escape(arg):
    if isinstance(arg, list):
        return [f"\"{s}\"" for s in arg]

    return f"\"{arg}\""
    
def _escape_on_windows(arg):
    if not os.name == "nt":
        return arg

    return _escape(arg)