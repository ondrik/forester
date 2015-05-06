#! /usr/bin/python3

import sys

# argument 1 - file with results

if len(sys.argv) != 2:
    print("./filter_false name_of_file_with_results_to_filter")
    sys.exit()

with open(sys.argv[1],'r') as f:
    lastline = ""
    for l in f:
        if "FALSE" in l:
            print(lastline.strip('\n'))
        lastline = l
