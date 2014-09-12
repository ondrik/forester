#! /bin/bash

cd libvata
git submodule init
git submodule update
make release
