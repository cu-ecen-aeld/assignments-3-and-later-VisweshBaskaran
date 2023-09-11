/*
File name: writer.c
File Description: This file contains the source code to process command line arguments and write the writestr(2nd argument) to file path writefile(1st argument) if it exists This .c file performs the functionality of writer.sh as done in A1.
Author: Visweshwaran Baskaran
Date: 09-10-2023

References:
	[1] https://www.scaler.com/topics/c/command-line-arguments-in-c/
	[2] Linux System Programming Talking Directly to the Kernel and C Library by Robert Love
	[3] https://stackoverflow.com/questions/33114152/what-to-do-if-a-posix-close-call-fails
	[4] https://www.gnu.org/software/libc/manual/html_node/Syslog-Example.html

*/
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define EXIT_ERROR 1
#define SYSCALL_ERROR -1
int main(int argc, char* argv[])
{
	
	char *writefile, *writestr;
	ssize_t write_nbytes;
	//Setting up syslog utility using LOG_USER facility. Using A2 as identification string
	openlog("A2",LOG_CONS | LOG_PID,LOG_USER);
	
	//Checking for number of arguments
	if(argc != 3)
	{
		syslog(LOG_ERR, "Invalid Number of Arguments. Entered %d arguments. Enter 2 arguments in the following order writefile writestr Error number: %d", argc, errno);
		return EXIT_ERROR;
		
	}
	
	// Storing arguments in variables after validating
	writefile = argv[1];
	writestr = argv[2];
	int fd;
	
	//creat(file) is the same system call as open (file, O_WRONLY | O_CREAT | O_TRUNC).
	//Second argument is an octal value 0664 gives user and group read, write permission and others read only permission. 
	fd = creat(writefile, 0664);
	
	//In case of failure to open file directory, creat returns -1 in case of error
	if (fd == SYSCALL_ERROR)
	{
		syslog(LOG_ERR, "File directory: %s does not exists. Error number: %d", writefile, errno);
		return EXIT_ERROR;
	}
	
	//writing writestr to fd, write() returns bytes written and returns -1 in case of error
	write_nbytes = write(fd, writestr, strlen(writestr));
	if(write_nbytes == SYSCALL_ERROR)
	{
		syslog(LOG_ERR, "Could not write: %s to %s. Error number: %d",writestr, writefile, errno);
		return EXIT_ERROR;
	}
	else
		syslog(LOG_DEBUG, "Writing %s to %s",writestr,writefile);
	
	closelog();
	if(close(fd) == SYSCALL_ERROR)
		syslog(LOG_ERR, "Error in closing file. Error number: %d", errno);
	return 0;
	
}
