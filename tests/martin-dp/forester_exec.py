#! /usr/bin/python3

import os
import subprocess

FILE_PARAM = 'file'
GCC_HOST='/usr/bin/gcc'
GCC_PLUG='/home/martin/forester/fa_build/libfa.so'
TOP_DIR=os.path.dirname(os.path.dirname(GCC_PLUG))
INCL_DIR=os.path.join(TOP_DIR, 'include', "forester-builtins")


def prepare(file_to_analyse):
    plugin_ops = ' ' # Note options has to end with space
    params=GCC_HOST+" -I"+INCL_DIR+" -O0 -DFORESTER -fplugin="+GCC_PLUG+' '+file_to_analyse
    p = subprocess.Popen(params.split(), stderr=subprocess.PIPE)

    return p

def prepare_and_execute(file_to_analyse):
    p = prepare(file_to_analyse)
    return p
