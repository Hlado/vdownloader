import benchmarking

import sys

if __name__ == "__main__":
    if len(sys.argv) != 6:
        sys.exit("Unexpected number of arguments") 

    file_url = benchmarking.video_file_url(link = sys.argv[3], format = sys.argv[4])
    
    benchmarking.run_benchmark(
        [
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url),
            benchmarking.vdownloader_command(path = sys.argv[2], url = file_url),
        ],
        times = sys.argv[5])