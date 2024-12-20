import benchmarking

import sys

if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit("Unexpected number of arguments") 

    file_url = benchmarking.video_file_url(link = sys.argv[2], format = sys.argv[3])
    
    benchmarking.run_benchmark(
        [
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url, options = ["-d", "0"]),
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url, options = ["-d", "1"]),
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url, options = ["-d", "4"])
        ],
        times = sys.argv[4])