/*
** http_client.cpp
*/
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <iterator>
#include <regex>
#include <string>

#define MAXDATASIZE 2000  // max number of bytes we can get at once

using namespace std;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
  int test = 0;
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];
  string url, host_name, port, path;

  if (argc != 2) {
    fprintf(stderr, "usage: client hostname\n");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  // use regular expression to parse the url
  url = argv[1];
  int nCount = count(url.begin(), url.end(), ':');
  smatch results;
  if (nCount == 1) {
    regex pattern("^http://([^/]+)(/.*)$");

    if (regex_match(url, results, pattern)) {
      cout << results.size() << endl;
      host_name = results[1];
      port = "80";
      path = results[2];
    } else {
      cout << "match failed: " << url << endl;
      return 2;
    }
  } else if (nCount == 2) {
    regex pattern("^http://([^:]+):([0-9]+)(/.*)$");
    if (regex_match(url, results, pattern)) {
      cout << results.size() << endl;
      host_name = results[1];
      port = results[2];
      path = results[3];
    } else {
      cout << "match failed: " << url << endl;
      return 2;
    }
  }

  cout << host_name << "\n" << port << "\n" << path << endl;

  if ((rv = getaddrinfo(host_name.data(), port.data(), &hints, &servinfo)) !=
      0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s,
            sizeof s);

  printf("client: connecting to %s\n", s);
  freeaddrinfo(servinfo);  // all done with this structure

  // format request string
  string req_str =
      "GET " + path +
      " HTTP/1.1\r\nUser-Agent: Wget/1.12(linux-gnu)\r\nHost: " + host_name +
      ":" + port + "\r\nConnection: Keep-Alive\r\n\r\n";

  // send request
  send(sockfd, req_str.c_str(), req_str.size(), 0);
  FILE *fp = NULL;

  fp = fopen("output", "wb");

  memset(buf, '\0', MAXDATASIZE);
  numbytes = recv(sockfd, buf, MAXDATASIZE, 0);

  if (numbytes > 0) {
    char *data_begin = strstr(buf, "\r\n\r\n") + 4;
    cout << buf;
    // write remaining part in the buffer expect header
    fwrite(data_begin, sizeof(char), strlen(data_begin), fp);
  } else {
    fclose(fp);
  }

  while (1) {
    memset(buf, '\0', MAXDATASIZE);
    numbytes = recv(sockfd, buf, MAXDATASIZE, 0);
    cout << buf;
    if (numbytes > 0) {
      fwrite(buf, sizeof(char), sizeof(char) * numbytes, fp);
    } else {
      fclose(fp);
      break;
    }
  }

  close(sockfd);

  return 0;
}
