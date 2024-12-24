import benchmarking

import sys

if __name__ == "__main__":
    num_args = len(sys.argv)
    if num_args != 6:
        sys.exit(f"Unexpected number of arguments ({num_args})") 

    file_url = benchmarking.video_file_url(link = sys.argv[3], format = sys.argv[4])
    
    benchmarking.run_benchmark(
        [
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url),
            benchmarking.vdownloader_command(path = sys.argv[2], url = file_url),
        ],
        times = sys.argv[5])
