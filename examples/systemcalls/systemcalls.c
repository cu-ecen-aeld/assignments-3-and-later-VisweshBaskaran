/*
File name: systemcalls.c
File Description:
Author: Visweshwaran Baskaran
Date:09-17-23
References:
	[1] Linux System Programming by Robert Love, 2nd Edition
	[2] ECEN5713 AESD Lecture Slides
	[3] Stack overflow post "wait(status), WEXITSTATUS(status) always returns 0 https://stackoverflow.com/questions/35471521/waitstatus-wexitstatusstatus-always-returns-0
	[4] Redirection inside call to execvp() not working https://stackoverflow.com/a/13784315/1446624

*/

#include "systemcalls.h"
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>

#define SYSCALL_ERROR -1
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

	int sys_return = system(cmd);
	if(sys_return != 0)
	return false;

    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
	pid_t pid;
	openlog("A3_do_exec", LOG_PID, LOG_USER);
	pid = fork();
	//On failure fork() returns -1
	if(pid == SYSCALL_ERROR)
	{
		syslog(LOG_ERR,"No child process created. Fork failed. Error no: %d",errno);
		return false;
	}
	if (pid == 0) //Child process
	{
	//Fork returns 0 in the child and PID in the parent on success
		syslog(LOG_DEBUG, "Child created");
		int ret = execv(command[0], command);
		if(ret == -1)
		{
			syslog(LOG_ERR, "execv() failed. Error no: %d", errno);
			exit(EXIT_FAILURE);
		}	
	}
	else if (pid > 0) //Parent process
	{
		int status;
		if(waitpid(pid, &status, 0) == -1)
		{
			syslog(LOG_ERR, "waitpid() failed. Error no: %d", errno);
			return false;		
		}
		else if (WIFEXITED(status)) //checking if child process exited normally
		{
			if (WEXITSTATUS(status) != 0) //checking for non zero status code, error in child process
			return false;
		}
		else
			return false;
	}

    	va_end(args);
    	closelog();

    	return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
	pid_t pid;
	openlog("A3_do_exec_redirect", LOG_PID, LOG_USER);
	int fd = creat(outputfile, 0664); 
	if (fd == SYSCALL_ERROR)
	{
		syslog(LOG_ERR, "File directory: %s does not exists. Error number: %d", outputfile, errno);
		return false;
	}
	pid = fork();
	switch(pid)
	{
	case -1:
		syslog(LOG_ERR,"No child process created. Fork failed. Error no: %d",errno);
		return false;
	case 0:
		//dup2(fd,1) copies file descriptor to file descript 1 which is stdout
		if(dup2(fd,1) < 0) //dup2() call returns -1 on error
		{
			syslog(LOG_ERR, "dup2() failed. Error no: %d", errno);
			return false;
		}
		close(fd);
		int ret = execv(command[0], command);
		if(ret == SYSCALL_ERROR)
		{
			syslog(LOG_ERR, "execv() failed. Error no: %d", errno);
			exit(EXIT_FAILURE);
		}
	default:
		close(fd);
		int status;
		if(waitpid(pid, &status, 0) == SYSCALL_ERROR) //waitpid() returns -1 on error
		{
			syslog(LOG_ERR, "waitpid() failed. Error no: %d", errno);
			return false;		
		}
		else if (WIFEXITED(status)) //checking if child process exited normally
		{
			if (WEXITSTATUS(status) != 0) //checking for non zero status code, error in child process
			return false;
		}
		else
			return false;	
	}
    	va_end(args);
    	closelog();
    	return true;
}


