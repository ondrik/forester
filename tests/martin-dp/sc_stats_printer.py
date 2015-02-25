#! /usr/bin/python3

TESTS_PASSED = "Tests OK:"
TESTS_S_FAILED = "Spurious tests failed:"
TESTS_R_FAILED = "Real tests failed:"

def num_ok(expected, get):
    return len(expected.intersection(get))

def sum_set_len(s1, s2):
    return len(s1.union(s2))

def get_ok_tests(expected_s, expected_r, s, r):
    return num_ok(expected_r, r) + num_ok(expected_s, s)

def get_spurious_failes_tests(expected_s, expected_r, s, r):
    return len(expected_s - s)

def get_real_failes_tests(expected_s, expected_r, s, r):
    return len(expected_r - r)

def compose_mssg(expected_s, expected_r, s, r, general_mssg, get_test_num, sum_all):
    return general_mssg + " " + "[" + str(get_test_num(expected_s, expected_r, s, r)) + "/" + str(sum_all(expected_r, expected_s)) + "]"

def prints(expected_s, expected_r, s, r):
    print (compose_mssg(expected_s, expected_r, s, r, TESTS_PASSED, get_ok_tests, sum_set_len))
    print (compose_mssg(expected_s, expected_r, s, r, TESTS_S_FAILED, get_spurious_failes_tests, (lambda r,s: len(s))))
    print (compose_mssg(expected_s, expected_r, s, r, TESTS_R_FAILED, get_real_failes_tests, lambda r,s: len(r)))
