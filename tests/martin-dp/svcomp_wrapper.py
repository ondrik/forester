#! /usr/bin/python3

import sys
import eval_std_res
from sc_tests_wrapper import execute_tests, get_real, get_spurious, get_unknown, eval_res
import sc_tests_wrapper
import sc_tests
import eval_structure

SPURIOUS_INFIX = '_true'
REAL_INFIX = '_false'


def should_be_spurious(test):
    return sc_tests.contains_suffix(test, SPURIOUS_INFIX)

def should_be_real(test):
    return sc_tests.contains_suffix(test, REAL_INFIX)

def get_tests_for_type(std_results, wanted_type):
    return [x[eval_structure.PATH] for x in std_results if x[eval_structure.TYPE] == wanted_type]

def get_tests(std_results):
    return [x[eval_structure.PATH] for x in std_results]

def run(path_to_tests):
    std_results = eval_std_res.eval_file(path_to_tests)
    
    FUNCT_DETECTION_MAP = {}
    FUNCT_DETECTION_MAP[sc_tests_wrapper.SPURIOUS_KEY] = should_be_spurious
    FUNCT_DETECTION_MAP[sc_tests_wrapper.REAL_KEY] = should_be_real
    br_results = execute_tests(get_tests(std_results), FUNCT_DETECTION_MAP)

    eval_res(
            set(get_tests_for_type(std_results, eval_structure.SPURIOUS)),
            set(get_tests_for_type(std_results, eval_structure.REAL)),
            set(get_spurious(br_results)),
            set(get_real(br_results)),
            set(get_unknown(br_results)))

if __name__ == '__main__':
    run(sys.argv[1])
