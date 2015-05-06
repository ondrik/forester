#! /bin/bash

# parameter 1 - file with list of files to be run with forester

function filter {
	for i in `cat $1 | grep false`
		do
			res=`../../fa_build/fagcc $i 2>/dev/null`
			if [ "`echo $res | grep $2`" != "" ]
			then
				echo "$i $2"
			elif [ "`echo $res | grep Assertion`" != "" ]
			then
				echo "$i ASSERT"
			else
				echo "$i FAILED"
			fi
		done
}

filter $1 "real"
filter $1 "spurious"
