#!/bin/sh

if [ $# -eq 2 ]
then
	echo "Valid Number of Arguments"
else
	echo "Invalid Number of Arguments"
	echo "Enter 2 arguments in the following order writefile writestr"
	exit 1
fi

dir="$(dirname "$1")"
#https://stackoverflow.com/questions/22737933/mkdirs-p-option
mkdir -p "$dir"
#https://phoenixnap.com/kb/how-to-create-a-file-in-linux
echo $2  > $1

if [ -f "$1" ]
then
	echo "File created"
else
	echo "File not created"
	exit 1
fi

exit 0 
