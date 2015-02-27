#! /usr/bin/python3

"""
This scipts iterates over the list of files given
as a command line parameter and executes Forester over it.
Then the Forester output is stored to a file given as the second parameter
"""

import sys
import os

import forester_exec

LIST_PARAM_INDEX = 1
FILE_PARAM_INDEX = 2

LIST_PARAM_KEY = 'LIST_KEY'
FILE_PARAM_KEY = 'FILE_KEY'

def get_params(cl_params):
    return {LIST_PARAM_KEY :cl_params[LIST_PARAM_INDEX], FILE_PARAM_KEY : cl_params[FILE_PARAM_INDEX]}

def str_to_bytes(string):
    return bytes(string, 'utf-8')

def write_delim(f):
    f.write(str_to_bytes("\n"))
    f.write(str_to_bytes("====================================\n"))
    f.write(str_to_bytes("====================================\n"))
    f.write(str_to_bytes("\n"))

def run_test(test, output):
    if os.path.exists(test):
        p = forester_exec.prepare_and_execute(test)
        with p.stderr as f:
            write_delim(output)
            output.write(str_to_bytes(test))
            write_delim(output)
            output.writelines(f.readlines())


def run_tests(params):
    with open(params[LIST_PARAM_KEY], 'r') as list_file, open(params[FILE_PARAM_KEY],'wb') as output:
        for line in list_file:
            run_test(line.strip(), output)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise(Exception("Not enough parameters"))
    run_tests(get_params(sys.argv))
