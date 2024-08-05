#!/bin/sh

set -e

#echo "0 = ${0}, 1 = ${1}, 2 = ${2}"

if [ ! $1 ] || [ ! $2 ]  
then
	echo "No arguments specified!!"
	exit 1
else
	filesdir=$1
	searchstr=$2
fi
		
if [ ! -d "${filesdir}" ] 
then
	echo "${filesdir} does not represent a directory on the filesystem"
	exit 1
else
	X=`ls ${filesdir} | wc -l`
	Y=`grep -sc "${searchstr}" ${filesdir}/* | awk -F ':' '{sum+=$2} END {print sum}'`
	echo "The number of files are ${X} and the number of matching lines are ${Y}"
fi
exit 0
