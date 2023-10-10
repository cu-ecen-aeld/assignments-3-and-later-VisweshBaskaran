/*
Author: Visweshwaran Baskaran
File name: aesdsocket.c
File description: 
This C program implements a simple socket server that listens on port 9000, accepts incoming connections, and logs received data to a file. It can be run in daemon mode using the '-d' command-line argument.
References: 
  [1] https://www.geeksforgeeks.org/signals-c-language/
  [2] https://beej.us/guide/bgnet/html/ 6.1 A Simple Stream Server
  [3] https://www.thegeekstuff.com/2012/02/c-daemon-process/
  [4] Linux System Programming Chapter 10: Signals Pg. 342 Examples

 */

#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>


#define PORT "9000" // Change the port to 9000
#define BACKLOG 10 // How many pending connections queue will hold
#define MAX_PACKET_SIZE 30000 // Maximum packet size, set as a large value instead of 1-24for sockettest.sh test cases
#define PATH "/var/tmp/aesdsocketdata"
#define SYSCALL_ERROR -1

// Declare socket file descriptor 
int sockfd; //declaring as global for signal handlers

/**
 * @brief Run the program as a daemon process.
 *
 * This function creates a daemon process by forking the parent process, sets it as a session leader,
 * redirects standard file descriptors, and changes the working directory to the root.
 */
void run_as_daemon() {
  pid_t process_id = 0;
  pid_t sid = 0;
  process_id = fork();
  if (process_id == -1) {
      perror("fork failed");
      exit(EXIT_FAILURE);
  }
  if (process_id > 0) {
      exit(EXIT_SUCCESS);
  }
  sid = setsid();
  if (sid == -1) //returns -1 on error
    {
      perror("sid");
      // Return failure
      exit(EXIT_FAILURE);
    }
  if (chdir("/") < 0) {
      perror("chdir failed");
      exit(EXIT_FAILURE);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  // Redirecting standard file descriptors to /dev/null
  int fd = open("/dev/null", O_RDWR);
  if (fd != -1) {
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > 2) {
          close(fd);
      }
  }
}

/**
 * @brief Signal handler function.
 *
 * Handles SIGINT and SIGTERM signals by logging and performing cleanup before exiting.
 *
 * @param sig The signal number.
 */
void signal_handler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
      syslog(LOG_INFO, "Caught signal, exiting");
      closelog();
      // Close the socket and delete the file
      close(sockfd);
      remove(PATH);
      exit(EXIT_SUCCESS);
  }
}

/**
 * @brief Get the IP address from a sockaddr structure.
 *
 * This function extracts and returns the IP address from a sockaddr structure.
 *
 * @param sa A pointer to a sockaddr structure.
 * @return A pointer to the extracted IP address.
 */
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in *) sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

/**
 * @brief Log information about an accepted connection.
 *
 * This function logs the IP address of the client from which the connection was accepted.
 *
 * @param their_addr The sockaddr_storage structure containing client information.
 */
void log_accepted_connection(struct sockaddr_storage their_addr) {
  char s[INET6_ADDRSTRLEN];
  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
  syslog(LOG_INFO, "Accepted connection from %s", s);
  printf("Accepted connection from %s\n", s);
}

/**
 * @brief Log information about a closed connection.
 *
 * This function logs the IP address of the client from which the connection was closed.
 *
 * @param their_addr The sockaddr_storage structure containing client information.
 */
void log_closed_connection(struct sockaddr_storage their_addr) {
  char s[INET6_ADDRSTRLEN];
  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
  syslog(LOG_INFO, "Closed connection from %s", s);
  printf("Accepted connection from %s\n", s);
}

int main(int argc, char *argv[]) 
{
	
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size = sizeof(their_addr);
  int yes = 1;
  int rv;
  ssize_t bytes_read;
  ssize_t bytes_recvd;
  bool daemon_mode = false;
  openlog("aesdsocket", LOG_PID, LOG_USER); // Open syslog
  //Cleanup of PATH
  remove(PATH); //to erase contents and the file in case of SIGKILL (kill -s 9 <pid>)
  if(signal(SIGINT, signal_handler) == SIG_ERR)
    {
      fprintf (stderr, "Cannot handle SIGINT!\n");
      exit(EXIT_FAILURE);
    }
  if(signal(SIGTERM, signal_handler) == SIG_ERR)
    {
      fprintf (stderr, "Cannot handle SIGTERM!\n");
      exit(EXIT_FAILURE);
    }
  // Checking if the -d argument is provided
  if ((argc == 2) && (strcmp(argv[1], "-d") == 0)) 
    daemon_mode = true;
  if (daemon_mode == true) 
    run_as_daemon();

  // Initialize the 'hints' structure to specify socket configuration options.
  memset(&hints, 0, sizeof(hints));
  // Set the address family to IPv4 (AF_INET) to ensure compatibility with IPv4 addresses.
  hints.ai_family = AF_INET;
  // Set the socket type to SOCK_STREAM, indicating a TCP socket.
  hints.ai_socktype = SOCK_STREAM;
  // Set the AI_PASSIVE flag, which indicates that the socket will be used for accepting incoming connections.
  hints.ai_flags = AI_PASSIVE;

  // Use getaddrinfo to retrieve a list of address structures that match the specified criteria.
  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      exit(EXIT_FAILURE);
  }

  // Iterate through the list of address structures to find a suitable one for binding.
  for (p = servinfo; p != NULL; p = p->ai_next) {
      // Create a socket using the address family, socket type, and protocol specified in the address structure.
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
          perror("server: socket");
          continue; // If socket creation fails, try the next address.
      }

      // Allow reusing the address/port even if it's in TIME_WAIT state.
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
          perror("setsockopt");
          exit(EXIT_FAILURE);
      }

      // Bind the socket to the address and port specified in the address structure.
      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
          close(sockfd);
          perror("server: bind");
          continue; // If binding fails, try the next address.
      }

      // If binding is successful, break out of the loop.
      break;
  }

  // Free the memory allocated by getaddrinfo.
  freeaddrinfo(servinfo);

  if (p == NULL) {
      fprintf(stderr, "server: failed to bind\n");
      exit(EXIT_FAILURE);
  }

  if (listen(sockfd, BACKLOG) == -1) {
      perror("listen");
      exit(EXIT_FAILURE);
  }
  /*
   * Server Main Loop:
   * This loop continuously accepts incoming connections, receives data from clients,
   * logs the accepted and closed connections, and sends the received data back to clients.
   * It handles errors and memory allocation failures gracefully.
   * Credit: Daniel Mendez and Ashwin Ravindra for logic and debugging
   */
  while (1) 
    {
      int new_sockfd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
      if (new_sockfd == -1) {
          perror("accept");
          continue;
      }
      log_accepted_connection(their_addr);

      // Log the accepted connection
      int file_fd = open(PATH, O_RDWR | O_APPEND | O_CREAT, 0666);
      if (file_fd == -1) {
          syslog(LOG_ERR,"Open failed: %s",strerror(errno));
          perror("open");
          exit(EXIT_FAILURE);
      }

      // Continuously receive data from the client
      while (1) {
          char *recv_buffer = (char * ) malloc(MAX_PACKET_SIZE);
          if (recv_buffer == NULL) {
              syslog(LOG_ERR,"Malloc failed: %s",strerror(errno));
              perror("malloc failed");
              exit(EXIT_FAILURE);
          }
          bytes_recvd = recv(new_sockfd, recv_buffer, MAX_PACKET_SIZE, 0);
          if (bytes_recvd == SYSCALL_ERROR) {
              perror("recv");
              syslog(LOG_ERR,"recv failed: %s",strerror(errno));
              free(recv_buffer); // Don't forget to free the buffer on error
              exit(EXIT_FAILURE);
          }
          if(write(file_fd, recv_buffer, bytes_recvd) == SYSCALL_ERROR)
            {
              perror("write");
              syslog(LOG_ERR,"write failed: %s",strerror(errno));
              free(recv_buffer);
              exit(EXIT_FAILURE);
            }
          // Check if the last character received is a newline
          if (recv_buffer[bytes_recvd - 1] == '\n') {
              free(recv_buffer);
              break;
          }
      }
      // Move the file cursor to the beginning of the file
      lseek(file_fd, 0, SEEK_SET);
      char * send_buffer = (char * ) malloc(MAX_PACKET_SIZE);
      if (send_buffer == NULL) {
          syslog(LOG_ERR,"Malloc failed: %s",strerror(errno));
          perror("malloc failed");
          exit(EXIT_FAILURE);
      }
      // Read data from the file into the send_buffer
      bytes_read = read(file_fd, send_buffer, MAX_PACKET_SIZE);
      if (bytes_read == SYSCALL_ERROR) {
          perror("read");
          exit(EXIT_FAILURE);
      }
      send(new_sockfd, send_buffer, bytes_read, 0);
      //close file descriptor of file
      close(file_fd);
      //close client socket file descriptor
      close(new_sockfd);
      free(send_buffer); 
      // Log the closed connection
      log_closed_connection(their_addr);

    }
  //close syslog
  closelog();
  exit(EXIT_SUCCESS);
  return 0;
}
