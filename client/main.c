//
//  main.c
//  client
//
//  Created by David Allison on 3/21/23.
//

#include <stdio.h>
#include "coroutine.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "dstring.h"
#include "vector.h"
#include <netdb.h>

typedef struct {
  int ipaddr;
  String* filename;
} ServerData;

static bool SendToServer(Coroutine* c,
                         int fd, const char* response, size_t length) {
  int offset = 0;
  while (length > 0) {
    // Wait until we can send to the network.  This will yield to other
    // coroutines and we will be resumed when we can write.
    CoroutineWait(c, fd, POLLOUT);
    ssize_t n = write(fd, response + offset, length);
    if (n == -1) {
      perror("write");
      return false;
    }
    if (n == 0) {
      return false;
    }
    length -= n;
    offset += n;
  }
  return true;
}

void Usage(void) {
  fprintf(stderr, "usage: client -j <jobs> <host> <filename>\n");
  exit(1);
}

void Client(Coroutine* c) {
  ServerData* data = CoroutineGetUserData(c);
  
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(80),
                             .sin_len = sizeof(int),
                             .sin_addr.s_addr = data->ipaddr};
  int e = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
  if (e != 0) {
    perror("connect");
    return;
  }
  String request = {0};
  StringPrintf(&request, "GET %s HTTP/1.1\r\n\r\n", data->filename->value);
  bool ok = SendToServer(c, fd, request.value, request.length);
  if (!ok) {
    fprintf(stderr, "Failed to send to server: %s\n", strerror(errno));
    close(fd);
    return;
  }
  char buf[256];
  for (;;) {
    CoroutineWait(c, fd, POLLIN);
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n == -1) {
      perror("read");
      close(fd);
      return;
    }
    if (n == 0) {
      printf("done\n");
      break;
    }
    fwrite(buf, 1, n, stdout);
  }
  close(fd);
}

int main(int argc, const char * argv[]) {
  String host = {};
  String filename = {};
  int num_jobs = 1;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (strcmp(argv[i], "-j")) {
        i++;
        if (i < argc) {
          num_jobs = atoi(argv[i]);
        }
      } else {
        Usage();
      }
    } else {
      if (host.length == 0) {
        StringSet(&host, argv[i]);
      } else if (filename.length == 0) {
        StringSet(&filename, argv[i]);
      } else {
        Usage();
      }
    }
  }
  if (host.length == 0 || filename.length == 0) {
    Usage();
  }
  
  CoroutineMachine m;
  CoroutineMachineInit(&m);

  struct hostent* entry = gethostbyname(host.value);
  if (entry == NULL) {
    fprintf(stderr, "unknown host %s\n", host.value);
    exit(1);
  }
  int ipaddr = ((struct in_addr*)entry->h_addr_list[0])->s_addr;
  ServerData server_data = {ipaddr = ipaddr, .filename = &filename};
  Vector clients = {0};
  for (int i = 0; i < num_jobs; i++) {
    Coroutine* client = NewCoroutine(&m, Client);
    CoroutineSetUserData(client, &server_data);
    VectorAppend(&clients, client);
    CoroutineStart(client);
  }

  // Run the main loop
  CoroutineMachineRun(&m);
  CoroutineMachineDestruct(&m);
  
  VectorDestructWithContents(&clients,
                             (VectorElementDestructor)CoroutineDestruct,
                             true);
}
