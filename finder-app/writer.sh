#!/bin/sh

# Author: Visweshwaran Baskaran
# Script name: writer.sh
# Description:
#	Shell script to take 2 arguments, full path to a file and a text string to be written within the file. Creates a new file with name and path writefile with content writestr, overwriting any 
#	existing file and creating the path if it doesn’t exist
# References:
#	1) https://stackoverflow.com/questions/22737933/mkdirs-p-option
# 	2) https://phoenixnap.com/kb/how-to-create-a-file-in-linux
#	3) https://unix.stackexchange.com/questions/734208/how-to-go-one-path-backward-and-store-the-path-in-variable

# Checking for number of arguments
if [ $# -eq 2 ]
then
	echo "Valid Number of Arguments"
else
	echo "Invalid Number of Arguments"
	echo "Enter 2 arguments in the following order writefile writestr"
	exit 1
fi

# Storing arguments in variables after validating
writefile=$1
writestr=$2

#extracting file path without .txt
dir="$(dirname "$writefile")"

#creating directory (-p to create parent directories if needed)
mkdir -p "$dir"
echo $writestr  > $writefile

# Checking if file was created
if [ -f "$writefile" ]
then
	echo "File created"
else
	echo "File not created"
	exit 1
fi
exit 0 
