//
//  main.c
//  client
//
//  Created by David Allison on 3/21/23.
//

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "buffer.h"
#include "coroutine.h"
#include "dstring.h"
#include "map.h"
#include "vector.h"

void Usage(void) {
  fprintf(stderr, "usage: client -j <jobs> <host> <filename>\n");
  exit(1);
}

typedef struct {
  const char* server_name; // Hostname of server.
  in_addr_t ipaddr;  // IP address for server (IPv4).
  String* filename;  // File to get (not owned by this struct).
} ServerData;

// Send data to the server from a coroutine.
static bool SendToServer(Coroutine* c, int fd, const char* request,
                         size_t length) {
  int offset = 0;
  const size_t kMaxLength = 1024;
  while (length > 0) {
    // Wait until we can send to the network.  This will yield to other
    // coroutines and we will be resumed when we can write.
    CoroutineWait(c, fd, POLLOUT);
    size_t nbytes = length;
    if (nbytes > kMaxLength) {
      nbytes = kMaxLength;
    }
    ssize_t n = write(fd, request + offset, nbytes);
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

static size_t ReadHeaders(Buffer* buffer, Vector* header, Map* http_headers) {
  // Parse the header.
  size_t i = 0;
  // Find the \r\n at the end of the first line.
  while (i < buffer->length && buffer->value[i] != '\r') {
    i++;
  }
  if (i == buffer->length) {
    // No header line.
    return i;
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
  return i;
}

static size_t ReadContents(Coroutine* c, int fd, Buffer* buffer, size_t i,
                           int length, bool write_to_output) {
  while (length > 0) {
    if (i < buffer->length) {
      // Data remaining in buffer
      size_t nbytes = buffer->length - i;
      if (nbytes > length) {
        nbytes = length;
      }
      if (write_to_output) {
        fwrite(&buffer->value[i], 1, nbytes, stdout);
      }
      length -= nbytes;
      i += nbytes;
    } else {
      // No data in buffer, read some more into the buffer.
      BufferClear(buffer);
      i = 0;
      char buf[256];
      CoroutineWait(c, fd, POLLIN);
      ssize_t n = read(fd, buf, sizeof(buf));
      if (n == -1) {
        perror("read");
        break;
      }
      if (n == 0) {
        printf("done\n");
        break;
      }
      BufferAppend(buffer, buf, n);
    }
  }
  return i;
}

static size_t ReadChunkLength(Coroutine* c, int fd, Buffer* buffer, size_t i,
                              int* length) {
  for (;;) {
    char ch;
    if (i < buffer->length) {
      ch = toupper(buffer->value[i++]);
    } else {
      // Fill the buffer with some more data.
      BufferClear(buffer);
      i = 0;
      CoroutineWait(c, fd, POLLIN);
      char buf[256];
      ssize_t n = read(fd, buf, sizeof(buf));
      if (n == -1) {
        perror("read");
        *length = 0;
        return i;
      }
      if (n == 0) {
        // Didn't read anything, EOF on input.
        return i;
      }
      BufferAppend(buffer, buf, n);
      continue;
    }
    if (ch == '\r') {
      i++;
      break;
    }
    if (ch > '9') {
      ch = ch - 'A' + 10;
    } else {
      ch -= '0';
    }
    *length = (*length << 4) | ch;
  }
  return i;
}

static void ReadChunkedContents(Coroutine* c, int fd, Buffer* buffer,
                                size_t i) {
  for (;;) {
    // First line is the length of the chunk in hex.
    int length = 0;
    i = ReadChunkLength(c, fd, buffer, i, &length);
    if (length == 0) {
      break;
    }
    i = ReadContents(c, fd, buffer, i, length, true);

    // Chunk is followed by a CRLF.  Don't print this, just skip it.
    i = ReadContents(c, fd, buffer, i, 2, false);
  }
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
    close(fd);
    perror("connect");
    return;
  }
  String request = {0};

  StringPrintf(&request, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n",
               data->filename->value, data->server_name);
  bool ok = SendToServer(c, fd, request.value, request.length);
  if (!ok) {
    fprintf(stderr, "Failed to send to server: %s\n", strerror(errno));
    close(fd);
    return;
  }

  Buffer buffer = {0};

  // Read incoming HTTP request and parse it.
  for (;;) {
    char buf[64];

    // Wait for data to arrive.  This will yield to other coroutines and
    // we will resume when data is available to read.
    CoroutineWait(c, fd, POLLIN);
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n == -1) {
      perror("read");
      close(fd);
      return;
    }
    if (n == 0) {
      // EOF while reading header, nothing we can do.
      close(fd);
      BufferDestruct(&buffer);
      return;
    }
    // Append to data buffer.
    BufferAppend(&buffer, buf, n);

    // A blank line terminates the read.
    if (strstr(buffer.value, "\r\n\r\n") != NULL) {
      break;
    }
  }

  Vector header = {0};
  Map http_headers;
  MapInitForCharPointerKeys(&http_headers);

  // The buffer contains the HTTP header line and the HTTP headers.
  size_t i = ReadHeaders(&buffer, &header, &http_headers);

  const size_t kProtocol = 0;
  const size_t kStatus = 1;
  const size_t kError = 2;

  // Make alises for the http header fields.
  String* status = header.value.p[kStatus];
  String* protocol = header.value.p[kProtocol];

  // Check for valid status.
  int status_value = atoi(status->value);
  if (status_value != 200) {
    fprintf(stderr, "%s Error: %d: ", protocol->value, status_value);
    // Print all error strings.
    const char* sep = "";
    for (size_t i = kError; i < header.length; i++) {
      String* s = header.value.p[i];
      fprintf(stderr, "%s%s", sep, s->value);
      sep = " ";
    }
    fprintf(stderr, "\n");
  } else {
    // We are the end of the http headers in the buffer.  We now need to work
    // out the length.  This is either from the CONTENT-LENGTH header or if
    // TRANSFER-ENCODING is "chunked", we have a series of chunks, each of which
    // is preceded by a hex length on a line of its own and terminated with a
    // CRLF
    MapKeyType k = {.p = "TRANSFER-ENCODING"};
    const char* transfer_encoding = MapFind(&http_headers, k);
    bool is_chunked = false;
    int content_length = -1;
    if (transfer_encoding != NULL &&
        strcmp(transfer_encoding, "chunked") == 0) {
      is_chunked = true;
    } else {
      MapKeyType k = {.p = "CONTENT-LENGTH"};
      const char* v = MapFind(&http_headers, k);
      if (v != NULL) {
        content_length = (int)strtoll(v, NULL, 10);
      }
    }

    // We use the buffer to hold all the data received, in blocks.
    if (is_chunked) {
      ReadChunkedContents(c, fd, &buffer, i);
    } else {
      if (content_length == -1) {
        fprintf(stderr,
                "Don't know how many bytes to read, no Content-length in "
                "headers\n");
      } else {
        ReadContents(c, fd, &buffer, i, content_length, true);
      }
    }
  }

  close(fd);
  BufferDestruct(&buffer);
  MapDestruct(&http_headers);
  VectorDestructWithContents(&header, (VectorElementDestructor)StringDestruct,
                             true);
}

int main(int argc, const char* argv[]) {
  String host = {};
  String filename = {};
  int num_jobs = 1;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (strcmp(argv[i], "-j") == 0) {
        // Allow -j N where N is a number
        i++;
        if (i < argc) {
          if (isdigit(argv[i][0])) {
            num_jobs = atoi(argv[i]);
          } else {
            Usage();
          }
        } else {
          Usage();
        }
      } else if (argv[i][1] == 'j' && isdigit(argv[i][2])) {
        // Allow -jN where N is a number.
        num_jobs = atoi(&argv[i][2]);
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

  struct hostent* entry = gethostbyname(host.value);
  if (entry == NULL) {
    fprintf(stderr, "unknown host %s\n", host.value);
    exit(1);
  }
  in_addr_t ipaddr = ((struct in_addr*)entry->h_addr_list[0])->s_addr;

  CoroutineMachine m;
  CoroutineMachineInit(&m);

  ServerData server_data = {.server_name = host.value,
    ipaddr = ipaddr, .filename = &filename};
  for (int i = 0; i < num_jobs; i++) {
    Coroutine* client = NewCoroutineWithUserData(&m, Client, &server_data);
    CoroutineStart(client);
  }

  // Run the main loop
  CoroutineMachineRun(&m);
  CoroutineMachineDestruct(&m);
}
