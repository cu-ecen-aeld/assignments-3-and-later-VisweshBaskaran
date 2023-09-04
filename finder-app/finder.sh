#!/bin/bash

# Author: Visweshwaran Baskaran
# Script name: finder.sh
# Description: 
# Shell script to take 2 arguments from user and find no. of files in that directory and subdirectory (path is first argument), and no. of occurences of string in same path (string is second argument)
# References:
# 	1) https://www.geeksforgeeks.org/how-to-count-files-in-directory-recursively-in-linux/
#	2) https://stackoverflow.com/questions/4121803/how-can-i-use-grep-to-find-a-wordinside-a-folder

# Checking for number of arguments
if [ $# -eq 2 ]
then
	echo "Valid Number of Arguments"
else
	echo "Invalid Number of Arguments"
	echo "Enter 2 arguments in the following order filesdir searchstr"
	exit 1
fi

# Checking if the file directory exists
if [ -d "$1" ]
then
	echo "File directory $1 exists "
else
	echo "File directory $1 does not exists"
	exit 1
fi

# Storing arguments in variables after validating
filesdir=$1
searchstr=$2
 
no_files=$(find $filesdir -type f | wc -l)
no_lines=$(grep -r $searchstr $filesdir | wc -l)

echo "The number of files are $no_files and the number of matching lines are $no_lines"
exit 0
