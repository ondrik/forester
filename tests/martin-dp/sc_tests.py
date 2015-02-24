#! /usr/bin/python3

import os

def load_files(path_to_files):
    return [ (path_to_files+f) for f in os.listdir(path_to_files) if os.path.isfile(os.path.join(path_to_files,f)) ]

def filter_suffix(list_of_file, suffix):
    return [f for f in list_of_file if f.find(suffix) == -1]

def only_suffix(list_of_file, suffix):
    return [f for f in list_of_file if f.find(suffix) != -1]

def contains_suffix(file_to_check, suffix):
    return file_to_check.find(suffix) != -1

def contains_substring(strings, substring):
    for s in strings:
        s = s.decode('UTF-8')
        if s.find(substring) != -1:
            return True

    return False

def files_without_suffix(test_dir, suffix):
    return filter_suffix(load_files(test_dir), suffix)

def files_with_suffix(test_dir, suffix):
    return only_suffix(load_files(test_dir), suffix)

def list_intersect(l1, l2):
    return set(l1).intersection(l2)
