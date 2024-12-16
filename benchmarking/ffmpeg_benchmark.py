import benchmarking

import sys

if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit("Unexpected number of arguments") 

    file_url = benchmarking.video_file_url(link = sys.argv[2], format = sys.argv[3])
    ffmpeg_script = benchmarking.path_to_script("invoke_ffmpeg.py")
    
    benchmarking.run_benchmark(
        [
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url),
            f"python \"{ffmpeg_script}\" \"{file_url}\""
        ],
        times = sys.argv[4])