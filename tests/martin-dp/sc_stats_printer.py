#! /usr/bin/python3

TESTS_PASSED = "Tests OK:"
TESTS_S_FAILED = "Spurious tests failed:"
TESTS_R_FAILED = "Real tests failed:"
TESTS_FAILED = "Tests failed due to a bug:"
TESTS_U= "Tests failed due to unknown result:"

def num_ok(expected, get):
    return len(expected.intersection(get))

def sum_set_len(s1, s2):
    return len(s1.union(s2))

def get_ok_tests(expected_s, expected_r, s, r, u):
    return num_ok(expected_r, r) + num_ok(expected_s, s)

def get_spurious_failes_tests(expected_s, expected_r, s, r, u):
    return len(expected_s - s)

def get_real_failes_tests(expected_s, expected_r, s, r, u):
    return len(expected_r - r)

def get_unknown(expected_s, expected_r, s, r, u):
    return len(u)

def get_all_failes_tests(expected_s, expected_r, s, r, u):
    return get_spurious_failes_tests(expected_s, expected_r, s, r, u) +\
        get_real_failes_tests(expected_s, expected_r, s, r, u) -\
        get_unknown(expected_s, expected_r, s, r, u)

def compose_mssg(expected_s, expected_r, s, r, u, general_mssg, get_test_num, sum_all):
    return general_mssg + " " + "[" + str(get_test_num(expected_s, expected_r, s, r, u)) + "/" + str(sum_all(expected_r, expected_s)) + "]"

def prints(expected_s, expected_r, s, r, u):
    print (compose_mssg(expected_s, expected_r, s, r, u, TESTS_S_FAILED, get_spurious_failes_tests, (lambda r,s: len(s))))
    print (compose_mssg(expected_s, expected_r, s, r, u, TESTS_R_FAILED, get_real_failes_tests, lambda r,s: len(r)))
    print ("---------------------------------")
    print (compose_mssg(expected_s, expected_r, s, r, u, TESTS_FAILED, get_all_failes_tests, sum_set_len))
    print (compose_mssg(expected_s, expected_r, s, r, u, TESTS_U, get_unknown, sum_set_len))
    print (compose_mssg(expected_s, expected_r, s, r, u, TESTS_PASSED, get_ok_tests, sum_set_len))
