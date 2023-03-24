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
  const size_t kMaxLength = 1024;
  while (length > 0) {
    // Wait until we can send to the network.  This will yield to other
    // coroutines and we will be resumed when we can write.
    CoroutineWait(c, data->fd, POLLOUT);
    size_t nbytes = length;
    if (nbytes > kMaxLength) {
      nbytes = kMaxLength;
    }
    ssize_t n = write(data->fd, response + offset, nbytes);
    if (n == -1) {
      perror("write");
      return;
    }
    if (n == 0) {
      return;
    }
    length -= n;
    offset += n;
  }
}

static void ReadHeaders(Buffer* buffer, Vector* header, Map* http_headers) {
  // Parse the header.
  size_t i = 0;
  // Find the \r\n at the end of the first line.
  while (i < buffer->length && buffer->value[i] != '\r') {
    i++;
  }
  if (i == buffer->length) {
    // No header line.
    return;
  }
  String header_line;
  StringInitFromSegment(&header_line, buffer->value, i);
  i += 2;  // Skip \r\n.

  // Parse the header line.  By splitting it at space into String objects
  // allocated from the heap.
  StringSplit(&header_line, ' ', header);

  // Don't need this now.
  StringDestruct(&header_line);

  // Extract MIME headers.  Holds pointers to the data in the buffer.  Does
  // not own the pointers.
  while (i < buffer->length) {
    if (buffer->value[i] == '\r') {
      // End of headers is a blank line.
      i += 2;
      break;
    }
    char* name = &buffer->value[i];
    while (i < buffer->length && buffer->value[i] != ':') {
      // Convert name to upper case as they are case insensitive.
      buffer->value[i] = toupper(buffer->value[i]);
      i++;
    }
    // No header value, end of headers.
    if (i == buffer->length) {
      break;
    }
    buffer->value[i] = '\0';  // Replace : by EOL
    i++;
    // Skip to non-space.
    while (i < buffer->length && isspace(buffer->value[i])) {
      i++;
    }
    char* value = &buffer->value[i];
    while (i < buffer->length) {
      if (i < buffer->length + 3 && buffer->value[i] == '\r') {
        // Check for continuation with a space as the first character on the
        // next line.  TAB too.
        if (buffer->value[i + 2] != ' ' && buffer->value[i + 2] != '\t') {
          break;
        }
      } else if (buffer->value[i] == '\r') {
        // No continuation, check for end of value.
        break;
      }
      i++;
    }
    buffer->value[i] = '\0';  // Replace \r by EOL
    i += 2;
    MapKeyValue kv = {.key.p = name, .value.p = value};
    MapInsert(http_headers, kv);
  }
}

void Server(Coroutine* c) {
  ClientData* data = CoroutineGetUserData(c);
  Buffer buffer = {0};

  // Read incoming HTTP request and parse it.
  for (;;) {
    char buf[64];

    // Wait for data to arrive.  This will yield to other coroutines and
    // we will resume when data is available to read.
    
    CoroutineWait(c, data->fd, POLLIN);
    ssize_t n = read(data->fd, buf, sizeof(buf));
    
    if (n == -1) {
      perror("read");
      close(data->fd);
      return;
    }
    if (n == 0) {
      // EOF while reading header, nothing we can do.
      close(data->fd);
      BufferDestruct(&buffer);
      return;
    }
    // Append to data buffer.
    BufferAppend(&buffer, buf, n);

    // A blank line terminates the read
    if (strstr(buffer.value, "\r\n\r\n") != NULL) {
      break;
    }
  }

  Vector header = {0};
  Map http_headers;
  MapInitForCharPointerKeys(&http_headers);

  ReadHeaders(&buffer, &header, &http_headers);

  // These are the indexes into the http_header for the fields.
  const size_t kMethod = 0;
  const size_t kFilename = 1;
  const size_t kProtocol = 2;

  // Make alises for the http header fields.
  String* method = header.value.p[kMethod];
  String* filename = header.value.p[kFilename];
  String* protocol = header.value.p[kProtocol];
  String response = {0};

  MapKeyType k = {.p = "HOST"};
  const char* hostname = MapFind(&http_headers, k);
  if (hostname == NULL) {
    hostname = "unknown";
  }
  printf("%s: %s for %s from %s\n", c->name.value, method->value,
         filename->value, hostname);

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
  MapDestruct(&http_headers);
  VectorDestructWithContents(&header, (VectorElementDestructor)StringDestruct,
                             true);
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

  // Enter a loop accepting incoming connections and spawning coroutines
  // to handle each one.  All coroutines run "in parallel", cooperating with
  // each other.  No threading here.
  for (;;) {
    // Wait for incoming connection.  This allows other coroutines to run
    // while we are waiting.
    CoroutineWait(c, s, POLLIN);

    // Allocate client data from the heap.  This will be passed to the
    // server coroutine, which will take ownership of it.
    ClientData* data = malloc(sizeof(ClientData));
    data->sender_len = sizeof(struct sockaddr_in);

    data->fd = accept(s, (struct sockaddr*)&data->sender, &data->sender_len);
    if (data->fd == -1) {
      perror("accept");
      free(data);
      continue;
    }

    // Make a coroutine to handle the connection. The coroutine
    // now owns the data.
    Coroutine* server = NewCoroutineWithUserData(c->machine, Server, data);

    // Start the coroutine.  It will be added to the list of coroutines that
    // are ready to run and will be run on the next yield or wait.
    CoroutineStart(server);
  }
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
