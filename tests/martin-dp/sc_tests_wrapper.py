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

FORESTER_SPURIOUS = "Is spurious"
FORESTER_REAL = "Is real"

def get_tests():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, C_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))

def get_spurious(t):
    assert(len(t) == 2)
    return t[0]

def get_real(t):
    assert(len(t) == 2)
    return t[1]

def should_be_spurioius(test):
    return sc_tests.contains_suffix(test, SPURIOUS_SUFFIX)

def should_be_real(test):
    return sc_tests.contains_suffix(test, REAL_SUFFIX)

def eval_test_res(test, eval_func):
    res = "[OK]" if eval_func(test) else "[FALSE]"
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

def execute_tests(tests):
    spurious = []
    real = []
    for test in tests:
        res_type = run_test(test)
        if res_type == eval_structure.SPURIOUS:
            spurious.append(test)
            eval_func = should_be_spurioius
        elif res_type == eval_structure.REAL:
            real.append(test)
            eval_func = should_be_real
        else:
            print("Unknown result for "+test)
            continue

        assert(eval_func == should_be_real or eval_func == should_be_spurioius)
        eval_test_res(test, eval_func)

    return (spurious, real)


def eval_res(expected_s, expected_r, s, r):
    print("---------------------------------")
    sc_stats_printer.prints(expected_s, expected_r, s, r)

def get_expected_spurious():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, SPURIOUS_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))

def get_expected_real():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, REAL_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))


def run_all_tests():
    results = execute_tests(get_tests())
    eval_res(
            set(get_expected_spurious()),
            set(get_expected_real()),
            set(get_spurious(results)),
            set(get_real(results)))

if __name__ == '__main__':
    run_all_tests()
