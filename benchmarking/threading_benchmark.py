import benchmarking

import sys

if __name__ == "__main__":
    num_args = len(sys.argv)
    if num_args != 5:
        sys.exit(f"Unexpected number of arguments ({num_args})") 

    file_url = benchmarking.video_file_url(link = sys.argv[2], format = sys.argv[3])
    
    benchmarking.run_benchmark(
        [
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url),
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url, options = ["-t", "0"]),
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url, options = ["-t", "8"])
        ],
        times = sys.argv[4])
