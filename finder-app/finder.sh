#!/bin/bash
# Checking for number of arguments

if [ $# -eq 2 ]
then
	echo "Valid Number of Arguments"
else
	echo "Invalid Number of Arguments"
	echo "Enter 2 arguments in the following order filesdir searchstr"
	exit 1
fi

if [ -d "$1" ]
then
	echo "File directory $1 exists "
else
	echo "File directory $1 does not exists"
	exit 1
fi

#refered https://www.geeksforgeeks.org/how-to-count-files-in-directory-recursively-in-linux/
no_files=$(find $1 -type f | wc -l)
#refered https://stackoverflow.com/questions/4121803/how-can-i-use-grep-to-find-a-word-inside-a-folder
no_lines=$(grep -r $2 $1 | wc -l)

echo "The number of files are $no_files and the number of matching lines are $no_lines"

exit 0
