import benchmarking

import sys

if __name__ == "__main__":
    num_args = len(sys.argv)
    if num_args < 6 or num_args > 8:
        sys.exit(f"Unexpected number of arguments ({num_args})") 

    file_url = benchmarking.video_file_url(link = sys.argv[3], format = sys.argv[4])
    
    ref_args = benchmarking.make_args('' if num_args < 7 else sys.argv[6])
    dev_args = benchmarking.make_args('' if num_args < 8 else sys.argv[7])
    
    benchmarking.run_benchmark(
        [ 
            benchmarking.vdownloader_command(path = sys.argv[1], url = file_url, options = dev_args),
            benchmarking.vdownloader_command(path = sys.argv[2], url = file_url, options = ref_args),
        ],
        times = sys.argv[5])
