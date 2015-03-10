#! /bin/bash

./sc_tests_wrapper.py sc/
./svcomp_wrapper.py res_db/res_heap_dp.txt
./svcomp_wrapper.py res_db/res_mem_dp.txt
