#! /usr/bin/python3

import functools
import sys

import sc_tests
import forester_exec
import sc_stats_printer
import eval_structure

TESTS_DIR="sc/"

SWP_SUFFIX="swp"

SPURIOUS_SUFFIX = '_S.c'
REAL_SUFFIX = '_R.c'
C_SUFFIX = '.c'

SPURIOUS_KEY = "SPUR"
REAL_KEY = "REAL"

FORESTER_SPURIOUS = "Is spurious"
FORESTER_REAL = "Is real"

UNKNOWN_MSG = "[Unknown]"
TRUE_MSG = "[OK]"
FALSE_MSG = "[False]"

def get_tests():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, C_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))

def get_spurious(t):
    assert(len(t) >= 1)
    return t[0]

def get_real(t):
    assert(len(t) >= 2)
    return t[1]

def get_unknown(t):
    assert(len(t) >= 3)
    return t[2]

def should_be_spurious(test):
    return sc_tests.contains_suffix(test, SPURIOUS_SUFFIX)

def should_be_real(test):
    return sc_tests.contains_suffix(test, REAL_SUFFIX)

def get_res_mggs(test, eval_func):
    return "[OK]" if eval_func(test) else "[FALSE]"

def print_test_res(test, res):
    print (test+" " +res)

def eval_res_type(lines):
    if (sc_tests.contains_substring(lines, FORESTER_SPURIOUS)):
        return eval_structure.SPURIOUS
    elif (sc_tests.contains_substring(lines, FORESTER_REAL)):
        return eval_structure.REAL
    return None

def run_test(test):
    p = forester_exec.prepare_and_execute(test)
    with p.stderr as f:
        lines = f.readlines()
        assert(lines != None)
        
        return eval_res_type(lines)

    return None

def execute_tests(tests, function_map):
    spurious = []
    real = []
    unknown = []
    for test in tests:
        res_type = run_test(test)
        if res_type == eval_structure.SPURIOUS:
            spurious.append(test)
            res_mssg = get_res_mggs(test, function_map[SPURIOUS_KEY])
        elif res_type == eval_structure.REAL:
            real.append(test)
            res_mssg = get_res_mggs(test, function_map[REAL_KEY])
        else:
            unknown.append(test)
            res_mssg = UNKNOWN_MSG

        assert(res_mssg != None)
        print_test_res(test, res_mssg)

    return (spurious, real, unknown)


def eval_res(expected_s, expected_r, s, r, u):
    print("---------------------------------")
    print("---------------------------------")
    sc_stats_printer.prints(expected_s, expected_r, s, r, u)

def get_expected_spurious():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, SPURIOUS_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))

def get_expected_real():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, REAL_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))


def run_all_tests():
    FUNCT_DETECTION_MAP = {}
    FUNCT_DETECTION_MAP[SPURIOUS_KEY] = should_be_spurious
    FUNCT_DETECTION_MAP[REAL_KEY] = should_be_real
    
    results = execute_tests(get_tests(), FUNCT_DETECTION_MAP)
    eval_res(
            set(get_expected_spurious()),
            set(get_expected_real()),
            set(get_spurious(results)),
            set(get_real(results)),
            set(get_unknown(results)))

if __name__ == '__main__':
    run_all_tests()
