#!/bin/sh

set -e

#echo "0 = ${0}, 1 = ${1}, 2 = ${2}"

if [ ! $1 ] || [ ! $2 ]  
then
	echo "No arguments specified!!"
	exit 1
else
	writefile=$1
	writestr=$2
fi

DIRNAME=`dirname ${writefile}`

if [ ! -d "$DIRNAME" ] 
then
	mkdir -p $DIRNAME
	if [ $? -ne 0 ]
	then
		echo "Failed to create $DIRNAME dirctory"
		exit 1
	fi
fi
echo "${writestr}" > ${writefile}
if [ $? -ne 0 ]
then
        echo "Failed to write ${writefile} file"
      	exit 1
fi
exit 0
