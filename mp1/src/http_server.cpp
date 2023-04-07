/*
** http_server.cpp
*/
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <regex>
#include <string>

using namespace std;  // invoke all identifiers defined in namespace std

void sigchld_handler(int s) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
  const int BACKLOG = 10;     // how many pending connections queue will hold
  const int BUFF_LEN = 8000;  // define the buff size to 8000
  int sockfd, new_fd;         // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;  // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;
  char *port_id = 0;
  char buff_request[BUFF_LEN], buff_response[BUFF_LEN];
  int size;
  FILE *file;

  if (argc != 2) {
    fprintf(stderr, "usage: serverport\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;  // use my IP
  port_id = argv[1];

  if ((rv = getaddrinfo(NULL, port_id, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    return 2;
  }

  freeaddrinfo(servinfo);  // all done with this structure

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler;  // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");

  while (1) {  // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    memset(buff_request, '\0', BUFF_LEN);

    if (!fork()) {    // this is the child process
      close(sockfd);  // child doesn't need the listener

      // This function is used to receive data from a connected
      // datagram or stream socket
      recv(new_fd, buff_request, BUFF_LEN, 0);

      string request(buff_request);  // initial a string
      string path;
      int end = request.find_first_of("\n");
      request = request.substr(0, end);
      cout << "Request: " << request << endl;
      regex pattern("GET /([^ ]*) HTTP/[0-9].[0-9][^ ]+");
      smatch match_results;
      if (regex_match(request, match_results, pattern)) {
        path = match_results[1];
        cout << "Path: " << path << endl;
      } else {
        request = "HTTP/1.1 400 Bad Request\r\n\r\n";
        if (send(new_fd, request.data(), request.size(), 0) == -1) {
          perror("send 400");
          exit(1);
        }
      }

      file = fopen(path.data(), "rb");

      if (file == NULL) {
        request = "HTTP/1.1 404 Not Found\r\n\r\n";
        if (send(new_fd, request.data(), request.size(), 0) == -1)
          perror("Error Send 404 Not Found Failed");
        perror("Error fopen failed");
        exit(1);
      }

      request = "HTTP/1.1 200 OK \r\n\r\n";
      if (send(new_fd, request.data(), request.size(), 0) == -1) {
        perror("Error Send 200 Failed");
        exit(1);
      }

      memset(buff_response, '\0', BUFF_LEN);
      while (true) {
        size = fread(buff_response, sizeof(char), BUFF_LEN, file);
        if (size == 0) break;
        if (send(new_fd, buff_response, size, 0) == -1) {
          perror("Send Error");
          exit(1);
        }
        memset(buff_response, '\0', BUFF_LEN);
      }

      fclose(file);
      close(new_fd);
      exit(0);
    }
    close(new_fd);  // parent doesn't need this
  }

  return 0;
}
