/*
File name: writer.c
File Description: This file contains the source code to process command line arguments and write the writestr(2nd argument) to file path writefile(1st argument) if it exists This .c file performs the functionality of writer.sh as done in A1.
Author: Visweshwaran Baskaran
Date: 09-10-2023

References:
	[1] https://www.scaler.com/topics/c/command-line-arguments-in-c/
	[2] Linux System Programming Talking Directly to the Kernel and C Library by Robert Love
	[3] https://stackoverflow.com/questions/33114152/what-to-do-if-a-posix-close-call-fails

*/
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define EXIT_ERROR 1

int main(int argc, char* argv[])
{
	
	char *writefile, *writestr;
	ssize_t write_nbytes;
	/*
	@brief Setting up syslog utility using LOG_USER facility
	@param priority: NULL
	@param option: LOG_PID
	@param facility: LOG_USER
	*/
	openlog(NULL,LOG_PID,LOG_USER);
	
	//Checking for number of arguments
	if(argc != 3)
	{
		syslog(LOG_ERR, "Invalid Number of Arguments.\nEntered %d arguments.\nEnter 2 arguments in the following order writefile writestr\nError number: %d", argc, errno);
		return EXIT_ERROR;
		
	}
	
	// Storing arguments in variables after validating
	writefile = argv[1];
	writestr = argv[2];
	int fd;
	
	fd = creat(writefile, 644);
	if (fd == -1)
	{
		syslog(LOG_ERR, "File directory: %s does not exists. Error number: %d", writefile, errno);
		return EXIT_ERROR;
	}
	
	write_nbytes = write(fd, writestr, strlen(writestr));
	if(write_nbytes == -1)
	{
		syslog(LOG_ERR, "Could not write: %s to %s. Error number: %d",writestr, writefile, errno);
		return EXIT_ERROR;
	}
	else
		syslog(LOG_DEBUG, "Writing %s to %s",writestr,writefile);
	
	closelog();
	if(close(fd) == -1)
		syslog(LOG_ERR, "Error in closing file. Error number: %d", errno);
	
	return 0;
	
}
