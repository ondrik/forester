#! /usr/bin/python3

import sys
import eval_std_res
from sc_tests_wrapper import execute_tests, get_real, get_spurious, eval_res
import sc_tests
import eval_structure

def get_expected_spurious(bw_results):
    return [x[eval_structure.PATH] for x in bw_results ]
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, SPURIOUS_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))

def get_expected_real():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, REAL_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))

def get_tests_for_type(std_results, wanted_type):
    return [x[eval_structure.PATH] for x in std_results if x[eval_structure.TYPE] == wanted_type]

def get_tests(std_results):
    return [x[eval_structure.PATH] for x in std_results]

def run(path_to_tests):
    std_results = eval_std_res.eval_file(path_to_tests)
  
    br_results = execute_tests(get_tests(std_results))
    eval_res(
            set(get_tests_for_type(std_results, eval_structure.SPURIOUS)),
            set(get_tests_for_type(std_results, eval_structure.REAL)),
            set(get_spurious(br_results)),
            set(get_real(br_results)))

if __name__ == '__main__':
    run(sys.argv[1])
