#! /bin/bash

# parameter 1 - file with results of forester

function printClass {
	cat $1 | grep $2
	realc=`cat $1 | grep $2 | wc -l`
	echo "$2 $realc"
}

./filter_false.py ../../fa/results/res_heap_bw.txt > temp.txt
./eval_filtered.sh temp.txt > temp1.txt
printClass temp1.txt "real"
printClass temp1.txt "spurious"
printClass temp1.txt "ASSERT"
printClass temp1.txt "FAILED"
rm temp.txt temp1.txt
