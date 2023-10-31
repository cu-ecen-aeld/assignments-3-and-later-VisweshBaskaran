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
[5] Linux manual pages https://linux.die.net/man
[6] queue.h leveraged from: https://raw.githubusercontent.com/freebsd/freebsd/stable/10/sys/sys/queue.h
[7] Unix timestamp: https://stackoverflow.com/questions/1551597/using-strftime-in-c-how-can-i-format-time-exactly-like-a-unix-timestamp
 */

#include "includes/queue.h"
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
#include <pthread.h>
#include <sys/time.h>

#define PORT "9000" // Change the port to 9000
#define BACKLOG 10 // How many pending connections queue will hold
#define MAX_PACKET_SIZE 30000 // Maximum packet size, set as a large value instead of 1024 for sockettest.sh test cases

#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
	#define PATH "/dev/aesdchar"
#else
	#define PATH "/var/tmp/aesdsocketdata"
#endif 

#define SYSCALL_ERROR - 1
#define TIMESTAMP_FORMAT "%Y %b %d %H:%M:%S" // RFC 2822 compliant strftime format

int sockfd; // declaring socket file descriptor as global for signal handlers
struct slist_data_s * datap = NULL; // for iterating
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
bool signal_received = false;

struct thread_data {
  pthread_t thread;
  int client_sockfd;
  struct sockaddr_storage client_addr;
  bool thread_complete_success;
};

#ifndef USE_AESD_CHAR_DEVICE
struct timer_data {
  pthread_t thread;
};
#endif

struct slist_data_s {
  struct thread_data connection_data;
  SLIST_ENTRY(slist_data_s) entries;
};
SLIST_HEAD(slisthead, slist_data_s)
head;

/**
 * @brief Get the IP address from a sockaddr structure.
 *
 * This function extracts and returns the IP address from a sockaddr structure.
 *
 * @param sa A pointer to a sockaddr structure.
 * @return A pointer to the extracted IP address.
 */
void * get_in_addr(struct sockaddr * sa) {
  if (sa -> sa_family == AF_INET) {
    return & (((struct sockaddr_in * ) sa) -> sin_addr);
  }

  return & (((struct sockaddr_in6 * ) sa) -> sin6_addr);
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
  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr * ) & their_addr), s, sizeof s);
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
  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr * ) & their_addr), s, sizeof s);
  syslog(LOG_INFO, "Closed connection from %s", s);
  printf("Accepted connection from %s\n", s);
}

/**
 * @brief Generates and appends timestamps to a file.
 *
 * @param thread_param A pointer to thread-specific data (struct thread_data) that may be used in the function.
 * @return NULL
 */
 #ifndef USE_AESD_CHAR_DEVICE
void * timestamp(void * thread_param) {
  if (thread_param == NULL) {
    // Handle invalid input gracefully
    perror("NULL params\n");
    return NULL;
  }
  //struct thread_data * thread_func_args = (struct thread_data * ) thread_param;
  int file_fd = open(PATH, O_RDWR | O_APPEND | O_CREAT, 0664);
  if (file_fd == -1) {
    syslog(LOG_ERR, "Open failed: %s", strerror(errno));
    perror("open");
    close(file_fd);
  }

  char fmt[64];
  struct timeval tv;
  struct tm * tm;

  while (!signal_received) {
    gettimeofday( & tv, NULL);
    if ((tm = localtime( & tv.tv_sec)) != NULL) {
      strftime(fmt, sizeof(fmt), "timestamp:%Y %b %d %H:%M:%S\n", tm);
    }
  
    if (pthread_mutex_lock( & mutex) != 0) {
      perror("mutex_lock");
      break;
    }
    ssize_t bytes_written = write(file_fd, fmt, strlen(fmt));
    if (pthread_mutex_unlock( & mutex) != 0) {
      perror("mutex_unlock");
      break;
    }

    if (bytes_written == -1) {
      perror("write");
      syslog(LOG_ERR, "write failed: %s", strerror(errno));
    }

    sleep(10);
  }
  close(file_fd);
  return NULL;
}
#endif

/**
 * @brief Thread function to handle client connections and log data to a file.
 *
 * @param thread_param A pointer to thread-specific data (struct thread_data) that may be used in the function.
 * @return EXIT_FAILURE on failure else NULL
 */
void * threadfunc(void * thread_param) {

  int file_fd = open(PATH, O_RDWR | O_APPEND | O_CREAT, 0664);
  if (file_fd == -1) {
    syslog(LOG_ERR, "Open failed: %s", strerror(errno));
    perror("open");
    exit(EXIT_FAILURE);
  }
  ssize_t bytes_read;
  ssize_t bytes_recvd;
  if (NULL == thread_param) {
    perror("NULL params\n");
    return NULL;
  }
   struct thread_data * thread_func_args = (struct thread_data * ) thread_param;
  log_accepted_connection(thread_func_args -> client_addr);
  while (1) {
    char * recv_buffer = (char * ) malloc(MAX_PACKET_SIZE);
    if (recv_buffer == NULL) {
      syslog(LOG_ERR, "Malloc failed: %s", strerror(errno));
      perror("malloc failed");
      //free(recv_buffer);
      goto exit_branch;
    }
    bytes_recvd = recv(thread_func_args -> client_sockfd, recv_buffer, MAX_PACKET_SIZE, 0);
    if (bytes_recvd == SYSCALL_ERROR) {
      perror("recv");
      syslog(LOG_ERR, "recv failed: %s", strerror(errno));
      free(recv_buffer); // Don't forget to free the buffer on error

    }
    #ifndef USE_AESD_CHAR_DEVICE
    if (pthread_mutex_lock( & mutex) != 0) {
      perror("mutex lock\n");
      free(recv_buffer);
      goto exit_branch;
    }
    #endif
    if (write(file_fd, recv_buffer, bytes_recvd) == SYSCALL_ERROR) {
      perror("write");
      //printf("bad file descriptor: %d", thread_func_args->file_fd);
      syslog(LOG_ERR, "write failed: %s", strerror(errno));
      free(recv_buffer);
      goto exit_branch;
    }
      #ifndef USE_AESD_CHAR_DEVICE
    if (pthread_mutex_unlock( & mutex) != 0) {
      perror("mutex unlock\n");
      free(recv_buffer);
      goto exit_branch;
    }
    #endif
    // Check if the last character received is a newline
    if (strchr(recv_buffer, '\n') != NULL) {
      free(recv_buffer);
      break;
    }
  }

  // Move the file cursor to the beginning of the file
  lseek(file_fd, 0, SEEK_SET);
  char * send_buffer = (char * ) malloc(MAX_PACKET_SIZE);
  if (send_buffer == NULL) {
    syslog(LOG_ERR, "Malloc failed: %s", strerror(errno));
    perror("malloc failed");
    free(send_buffer);
    goto exit_branch;
  }
  // Read data from the file into the send_buffer
  bytes_read = read(file_fd, send_buffer, MAX_PACKET_SIZE);
  if (bytes_read == SYSCALL_ERROR) {
    perror("read");
    free(send_buffer);
    goto exit_branch;
  }
  send(thread_func_args -> client_sockfd, send_buffer, bytes_read, 0);
  // Log the closed connection
  log_closed_connection(thread_func_args -> client_addr);

  exit_branch:
    // close client socket file descriptor
    close(thread_func_args -> client_sockfd);
  thread_func_args -> thread_complete_success = true;
  close(file_fd);
  if (send_buffer)
    free(send_buffer);
  return NULL; /*for avoiding "error: control reaches end of non-void function"*/

}
/**
 * @brief Gracefully exits the program, cleaning up resources and closing connections.
 * @param
 * @return none
 */
void exit_gracefully() {

  syslog(LOG_INFO, "Caught signal, exiting");
  //closelog();
  // Close the socket and delete the file
    // Join the timer thread
    //pthread_join(timer_data.thread, NULL);
  close(sockfd);
    #ifndef USE_AESD_CHAR_DEVICE
  remove(PATH);
  #endif
  while (SLIST_FIRST( & head) != NULL) {
  struct slist_data_s *datap;
    SLIST_FOREACH(datap, & head, entries) {
      close(datap -> connection_data.client_sockfd);
      pthread_join(datap -> connection_data.thread, NULL);
      SLIST_REMOVE( & head, datap, slist_data_s, entries);
      free(datap);
    }
  }
  
    pthread_mutex_destroy(&mutex);
    closelog();
  //exit(EXIT_SUCCESS);
}

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
  if (sid == -1) // returns -1 on error
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
    signal_received = true;
    exit_gracefully();
  }
}

int main(int argc, char * argv[]) {
  pthread_mutex_init( & mutex, NULL);
  struct addrinfo hints, * servinfo, * p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size = sizeof(their_addr);
  int yes = 1;
  int rv;

  bool daemon_mode = false;
  openlog("aesdsocket", LOG_PID, LOG_USER); // Open syslog
  // Cleanup of PATH
    #ifndef USE_AESD_CHAR_DEVICE
  remove(PATH); // to erase contents and the file in case of SIGKILL (kill -s 9 <pid>)
#endif
  struct sigaction sa;
  sa.sa_handler = & signal_handler; // reap all dead processes
  sigemptyset( & sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, & sa, NULL) == -1) {
    closelog();
    pthread_mutex_destroy( & mutex);
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGTERM, & sa, NULL) == -1) {
    closelog();
    pthread_mutex_destroy( & mutex);
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  // Checking if the -d argument is provided
  if ((argc == 2) && (strcmp(argv[1], "-d") == 0))
    daemon_mode = true;
  if (daemon_mode == true)
    run_as_daemon();

  // Initialize the 'hints' structure to specify socket configuration options.
  memset( & hints, 0, sizeof(hints));
  // Set the address family to IPv4 (AF_INET) to ensure compatibility with IPv4 addresses.
  hints.ai_family = AF_INET;
  // Set the socket type to SOCK_STREAM, indicating a TCP socket.
  hints.ai_socktype = SOCK_STREAM;
  // Set the AI_PASSIVE flag, which indicates that the socket will be used for accepting incoming connections.
  hints.ai_flags = AI_PASSIVE;

  // Use getaddrinfo to retrieve a list of address structures that match the specified criteria.
  if ((rv = getaddrinfo(NULL, PORT, & hints, & servinfo)) != 0) {
    closelog();
    pthread_mutex_destroy( & mutex);
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(EXIT_FAILURE);
  }

  // Iterate through the list of address structures to find a suitable one for binding.
  for (p = servinfo; p != NULL; p = p -> ai_next) {
    // Create a socket using the address family, socket type, and protocol specified in the address structure.
    if ((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
      perror("server: socket");
      continue; // If socket creation fails, try the next address.
    }

    // Allow reusing the address/port even if it's in TIME_WAIT state.
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int)) == -1) {
      closelog();
      pthread_mutex_destroy( & mutex);
      perror("setsockopt");
      exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port specified in the address structure.
    if (bind(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
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
    closelog();
    pthread_mutex_destroy( & mutex);
    fprintf(stderr, "server: failed to bind\n");
    exit(EXIT_FAILURE);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    closelog();
    pthread_mutex_destroy( & mutex);
    perror("listen");
    exit(EXIT_FAILURE);
  }

  SLIST_INIT( & head);
   #ifndef USE_AESD_CHAR_DEVICE
  struct timer_data timer_data_t;
  pthread_create( & (timer_data_t.thread), NULL, timestamp, & timer_data_t);
  #endif
  while (signal_received == false) 
  {

    int client_sockfd = accept(sockfd, (struct sockaddr * ) & their_addr, & sin_size);
    if (client_sockfd == -1) {
      perror("accept");
      continue;
    }
    datap = (struct slist_data_s * ) malloc(sizeof(struct slist_data_s));

    datap -> connection_data.client_sockfd = client_sockfd;
    datap -> connection_data.client_addr = their_addr;
    datap -> connection_data.thread_complete_success = false;
     SLIST_INSERT_HEAD(&head, datap, entries);
    if (pthread_create( & (datap -> connection_data.thread), NULL, threadfunc, & datap -> connection_data) != 0) {
      perror("pthread_create");
      break;
    }
    struct slist_data_s * temp;
    SLIST_FOREACH_SAFE(datap, & head, entries, temp) {
      if (datap -> connection_data.thread_complete_success) {
        pthread_join(datap -> connection_data.thread, NULL);
        SLIST_REMOVE( & head, datap, slist_data_s, entries);
        free(datap);
      }
    }

  }

  while (SLIST_FIRST( &head) != NULL) {
    SLIST_FOREACH(datap, & head, entries) {
      close(datap -> connection_data.client_sockfd);
      pthread_join(datap -> connection_data.thread, NULL);
      SLIST_REMOVE( & head, datap, slist_data_s, entries);
      free(datap);
    }
    
  }
  #ifndef USE_AESD_CHAR_DEVICE
  pthread_join((timer_data_t.thread), NULL);
  #endif
  pthread_mutex_destroy( &mutex);
  #ifndef USE_AESD_CHAR_DEVICE
  remove(PATH);
   #endif
  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);
  free(datap);
  closelog();
  return 0;
}
