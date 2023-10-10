/*
Reference: 	
	[1] https://www.geeksforgeeks.org/signals-c-language/
	[2] https://beej.us/guide/bgnet/html/
	[3] https://www.thegeekstuff.com/2012/02/c-daemon-process/
*/

#include <stdio.h>

#include <stdlib.h>

#include <unistd.h>

#include <errno.h>

#include <string.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <netdb.h>

#include <arpa/inet.h>

#include <sys/wait.h>

#include <signal.h>

#include <syslog.h>

#include <fcntl.h>

#define PORT "9000" // Change the port to 9000
#define BACKLOG 10 // how many pending connections queue will hold
#define MAX_PACKET_SIZE 1000000 // Maximum packet size 1024
#define PATH "/var/tmp/aesdsocketdata"

int sockfd; // Declare sockfd as a global variable for signal handling
void run_as_daemon() {
  pid_t process_id = 0;
  pid_t sid = 0;
  process_id = fork();
  // Indication of fork() failure
  if (process_id == -1) {
    perror("fork failed");
    // Return failure in exit status
    exit(EXIT_FAILURE);
  }
  if (process_id > 0) {
    exit(EXIT_SUCCESS);
  }
  sid = setsid();
  if (sid < 0) {
    // Return failure
    exit(EXIT_FAILURE);
  }
  if (chdir("/") < 0)
  {
    perror("chdir failed");
    exit(EXIT_FAILURE);
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}
void signal_handler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    syslog(LOG_INFO, "Caught signal, exiting");
    closelog();
    // Close the socket and delete the file
    close(sockfd);
    //remove(PATH);
    exit(EXIT_SUCCESS);
  }
}

void * get_in_addr(struct sockaddr * sa) {
  if (sa -> sa_family == AF_INET) {
    return & (((struct sockaddr_in * ) sa) -> sin_addr);
  }

  return & (((struct sockaddr_in6 * ) sa) -> sin6_addr);
}

void log_accepted_connection(struct sockaddr_storage their_addr) {
  char s[INET6_ADDRSTRLEN];
  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr * ) & their_addr), s, sizeof s);
  syslog(LOG_INFO, "Accepted connection from %s", s);
}

void log_closed_connection(struct sockaddr_storage their_addr) {
  char s[INET6_ADDRSTRLEN];
  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr * ) & their_addr), s, sizeof s);
  syslog(LOG_INFO, "Closed connection from %s", s);
}

int main(int argc, char *argv[]) {
ssize_t bytes_read;
ssize_t bytes_recvd;
  int daemon_mode = 0;

  // Check if the -d argument is provided
  if (argc == 2 && strcmp(argv[1], "-d") == 0) {
    daemon_mode = 1;
  }
  if (daemon_mode == 1) {

    run_as_daemon();

  }
  struct addrinfo hints, * servinfo, * p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size = sizeof(struct sockaddr_storage);
  int yes = 1;
  int rv;


  memset( & hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  openlog("aesdsocket", LOG_PID, LOG_USER); // Open syslog
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  if ((rv = getaddrinfo(NULL, PORT, & hints, & servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(EXIT_FAILURE);
  }

  for (p = servinfo; p != NULL; p = p -> ai_next) {
    if ((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(EXIT_FAILURE);
    }

    if (bind(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(EXIT_FAILURE);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  while (1) 
  {
    int new_sockfd = accept(sockfd, (struct sockaddr * ) & their_addr, &sin_size);
    if (new_sockfd == -1) 
    {
      perror("accept");
      continue;
    }
    log_accepted_connection(their_addr);

    // Log the accepted connection
    int file_fd = open(PATH, O_RDWR | O_APPEND | O_CREAT, 0666);
    if (file_fd == -1) {
      perror("open");
      exit(EXIT_FAILURE);
    }

    while (1) {
      char *recv_buffer = (char * ) malloc(MAX_PACKET_SIZE);
      if (recv_buffer == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
      }
      memset(recv_buffer, '\0', MAX_PACKET_SIZE);
      bytes_recvd = recv(new_sockfd, recv_buffer, MAX_PACKET_SIZE, 0);
      write(file_fd, recv_buffer, bytes_recvd);
      if (recv_buffer[MAX_PACKET_SIZE - 1] == '\n')
      {
       free(recv_buffer);
        break;
      }
    }
    lseek(file_fd, 0, SEEK_SET);
    char * send_buffer = (char * ) malloc(MAX_PACKET_SIZE);
    if (send_buffer == NULL) {
      perror("malloc failed");
      exit(EXIT_FAILURE);
    }
    memset(send_buffer, '\0', MAX_PACKET_SIZE);
    bytes_read = read(file_fd, send_buffer, MAX_PACKET_SIZE);
    if (bytes_read == -1) {
      perror("read");
      exit(EXIT_FAILURE);
    }
    send(new_sockfd, send_buffer, bytes_read, 0);
    // Close the sockfd and perform cleanup here
    close(new_sockfd);
    close(file_fd);
    // Log the closed connection
    log_closed_connection(their_addr);
    closelog();
    free(send_buffer); // Free the send_buffer memory

  }
  exit(EXIT_SUCCESS);
  return 0;
}
