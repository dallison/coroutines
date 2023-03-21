//
//  main.c
//  http
//
//  Created by David Allison on 3/20/23.
//

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "buffer.h"
#include "coroutine.h"
#include "dstring.h"
#include "map.h"

// Data about a client, passed to server coroutines as user data.
typedef struct {
  int fd;                     // Fd for socket to read/write.
  struct sockaddr_in sender;  // Client's address.
  socklen_t sender_len;       // Length of client's address.
} ClientData;

// Send a buffer full of data to the coroutines file descriptor.
static void SendToClient(Coroutine* c, const char* response, size_t length) {
  ClientData* data = CoroutineGetUserData(c);
  int offset = 0;
  while (length > 0) {
    CoroutineWait(c, data->fd, POLLOUT);
    ssize_t n = write(data->fd, response + offset, length);
    if (n == -1) {
      perror("write");
      close(data->fd);
      return;
    }
    if (n == 0) {
      close(data->fd);
      return;
    }
    length -= n;
    offset += n;
  }
}

void Server(Coroutine* c) {
  printf("Coroutine %s started\n", c->name.value);
  ClientData* data = CoroutineGetUserData(c);
  Buffer buffer = {0};

  // Read incoming HTTP request and parse it.
  for (;;) {
    char buf[64];

    // Wait for data to arrive.
    CoroutineWait(c, data->fd, POLLIN);
    ssize_t n = read(data->fd, buf, sizeof(buf));
    if (n == -1) {
      perror("read");
      close(data->fd);
      return;
    }
    if (n == 0) {
      // EOF
      close(data->fd);
      return;
    }
    // Append to data buffer.
    BufferAppend(&buffer, buf, n);

    // A blank line terminates the read
    if (strstr(buffer.value, "\r\n\r\n") != NULL) {
      break;
    }
  }

  // Parse the header.
  size_t i = 0;
  // Find the \r\n at the end of the first line
  while (i < buffer.length && buffer.value[i] != '\r') {
    i++;
  }
  String header;
  StringInitFromSegment(&header, buffer.value, i);
  i += 2;  // Skip \r\n.

  // Parse the header line.  By splitting it at space into String objects
  // allocated from the heap.
  Vector http_header = {0};
  StringSplit(&header, ' ', &http_header);

  // These are the indexes into the http_header for the fields.
  const size_t kMethod = 0;
  const size_t kFilename = 1;
  const size_t kProtocol = 2;

  // Make alises for the http header fields.
  String* method = http_header.value.p[kMethod];
  String* filename = http_header.value.p[kFilename];
  String* protocol = http_header.value.p[kProtocol];

  // Don't need this now.
  StringDestruct(&header);

  // Extract MIME headers.  Holds pointers to the data in the buffer.  Does
  // not own the pointers.
  Map mime_headers;
  MapInitForCharPointerKeys(&mime_headers);
  while (i < buffer.length) {
    char* name = &buffer.value[i];
    while (i < buffer.length && buffer.value[i] != ':') {
      i++;
    }
    // No header value, end of headers.
    if (i == buffer.length) {
      break;
    }
    buffer.value[i] = '\0';  // Replace : by EOL
    i++;
    // Skip to non-space.
    while (i < buffer.length && isspace(buffer.value[i])) {
      i++;
    }
    char* value = &buffer.value[i];
    while (i < buffer.length && buffer.value[i] != '\r') {
      i++;
    }
    buffer.value[i] = '\0';  // Replace \r by EOL
    i += 2;
    MapKeyValue kv = {.key.p = name, .value.p = value};
    MapInsert(&mime_headers, kv);
  }

  String response = {0};

  // Only support the GET method for now.
  if (StringEqual(method, "GET")) {
    struct stat st;
    int e = stat(filename->value, &st);
    if (e == -1) {
      StringPrintf(&response, "%s 404 Not Found\r\n\r\n", protocol->value);
      SendToClient(c, response.value, response.length);
    } else {
      int file_fd = open(filename->value, O_RDONLY);
      if (file_fd == -1) {
        StringPrintf(&response, "%s 404 Not Found\r\n\r\n", protocol->value);
        SendToClient(c, response.value, response.length);
      } else {
        // Send the file back.
        StringPrintf(&response,
                     "%s 200 OK\r\nContent-type: text/html\r\nContent-length: "
                     "%zd\r\n\r\n",
                     protocol->value, st.st_size);
        SendToClient(c, response.value, response.length);

        for (;;) {
          char buf[1024];
          CoroutineWait(c, file_fd, POLLIN);
          ssize_t n = read(file_fd, buf, sizeof(buf));
          if (n == -1) {
            perror("file read");
            break;
          }
          if (n == 0) {
            break;
          }
          SendToClient(c, buf, n);
        }
        close(file_fd);
      }
    }
  } else {
    // Invalid request method.
    StringPrintf(&response, "%s 400 Invalid request method\r\n\r\n",
                 protocol->value);
    SendToClient(c, response.value, response.length);
  }

  close(data->fd);
  free(data);
  BufferDestruct(&buffer);
  MapDestruct(&mime_headers);
  VectorDestructWithContents(&http_header,
                             (VectorElementDestructor)StringDestruct, true);
  printf("Coroutine %s ended\n", c->name.value);
}

void Listener(Coroutine* c) {
  int s = socket(PF_INET, SOCK_STREAM, 0);
  if (s == -1) {
    perror("socket");
    return;
  }
  int val = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(80),
                             .sin_len = sizeof(int),
                             .sin_addr.s_addr = INADDR_ANY};
  int e = bind(s, (struct sockaddr*)&addr, sizeof(addr));
  if (e == -1) {
    perror("bind");
    close(s);
    return;
  }
  listen(s, 10);

  // Enter a loop accepting incomign connections and spawning coroutines
  // to handle each one.  All coroutines run "in parallel", cooperating with
  // each other.  No threading here.
  for (;;) {
    // Wait for incoming connection.
    CoroutineWait(c, s, POLLIN);

    ClientData* data = malloc(sizeof(ClientData));
    data->sender_len = sizeof(struct sockaddr_in);

    data->fd = accept(s, (struct sockaddr*)&data->sender, &data->sender_len);
    if (data->fd == -1) {
      perror("accept");
      break;
    }

    // Make a coroutine to handle the connection.
    Coroutine* server = NewCoroutine(c->machine, Server);

    // Put the ClientData into the coroutine's user data.  The coroutine
    // now owns the data.
    CoroutineSetUserData(server, data);

    // Start the coroutine.
    CoroutineStart(server);
  }
  close(s);
}

int main(int argc, const char* argv[]) {
  CoroutineMachine m;
  CoroutineMachineInit(&m);

  Coroutine listener;
  CoroutineInit(&listener, &m, Listener);
  CoroutineStart(&listener);

  // Run the main loop
  CoroutineMachineRun(&m);
  CoroutineMachineDestruct(&m);
}
