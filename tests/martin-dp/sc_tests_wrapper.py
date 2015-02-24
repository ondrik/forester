#! /usr/bin/python3

import functools

import sc_tests
import forester_exec

TESTS_DIR="sc/"

SWP_SUFFIX="swp"

YES_SUFFIX = '_Y.c'
NO_SUFFIX = '_N.c'
C_SUFFIX = '.c'

IS_SPURIOUS_MSSG = "is spurious"
IS_NOT_SPURIOUS_MSSG = "is not spurious"

FORESTER_SPURIOUS = "Is spurious"
FORESTER_REAL = "Is real"

OK_SPURIOUS = "All spurious examples detected successful"
FALSE_SPURIOUS = "These files should be spurious"
SHOULD_REAL = "These files should be real"

def get_tests():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, C_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))

def get_spurious(t):
    return t[0]

def get_real(t):
    return t[1]

def execute_tests():
    spurious = []
    real = []
    for test in get_tests():
        p = forester_exec.prepare_and_execute(test)
        with p.stderr as f:
            lines = f.readlines()
            if (sc_tests.contains_substring(lines, FORESTER_SPURIOUS)):
                spurious.append(test)
            elif (sc_tests.cntains_substring(lines, FORESTER_REAL)):
                real.append(test)
            else:
                raise("This should never happen")

    return (spurious, real)

def eval_res(expected_y, expected_n, y, n):
    if (expected_y != y and len(expected_y - y)):
        print(FALSE_SPURIOUS)
        print(expected_y - y)

    if (expected_n != n and len(expected_n - n)):
        print(SHOULD_REAL)
        print(expected_n - n)

    print(OK_SPURIOUS)

def get_expected_spurious():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, YES_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))

def get_expected_real():
    return sc_tests.list_intersect(
            sc_tests.files_with_suffix(TESTS_DIR, NO_SUFFIX),
            sc_tests.files_without_suffix(TESTS_DIR, SWP_SUFFIX))


def run_all_tests():
    results = execute_tests()
    eval_res(
            set(get_expected_spurious()),
            set(get_expected_real()),
            set(get_spurious(results)),
            set(get_real(results)))

run_all_tests()
