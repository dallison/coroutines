//
//  main.c
//  coroutines
//
//  Created by David Allison on 3/13/23.
//

#include <stdio.h>
#include "coroutine.h"

#if defined(__APPLE__)
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

#include <unistd.h>

#if defined(__linux__)
#include <sys/timerfd.h>
#endif

int pipes[2];

#if defined(__APPLE__)
static void NewTimer(int kq, int millis) {
  struct kevent e;

  EV_SET(&e, 1, EVFILT_TIMER, EV_ADD, 0, millis, 0);
  kevent(kq, &e, 1, NULL, 0, NULL);
}

static void ClearTimer(int kq) {
  struct kevent e;

  EV_SET(&e, 1, EVFILT_TIMER, EV_DELETE, 0, 0, 0);
  kevent(kq, &e, 1, NULL, 0, NULL);
}
#elif defined(__linux__)

static void NewTimer(int fd, int millis) {}

static void ClearTimer(int fd) {
  int64_t val;
  (void)read(fd, &val, 8);
}
#endif

void Generator(Coroutine* c) {
  for (int i = 1; i < 5; i++) {
    CoroutineYieldValue(c, &i);
  }
}

void Co1(Coroutine* c) {
  Coroutine generator;
  CoroutineInit(&generator, c->machine, Generator);
#if defined(__APPLE__)
  int fd = kqueue();
#elif defined(__linux__)
  struct itimerspec new_value;
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  new_value.it_value.tv_sec = now.tv_sec;
  new_value.it_value.tv_nsec = now.tv_nsec;
  new_value.it_interval.tv_sec = 0;
  new_value.it_interval.tv_nsec = 100000000;
  int fd = timerfd_create(CLOCK_REALTIME, 0);
  timerfd_settime(fd, TFD_TIMER_ABSTIME, &new_value, NULL);

#endif
  while (CoroutineIsAlive(c, &generator)) {
    int value = 0;
    CoroutineCall(c, &generator, &value, sizeof(value));
    if (CoroutineIsAlive(c, &generator)) {
      printf("Value: %d\n", value);
      NewTimer(fd, 100);
      CoroutineWait(c, fd, POLLIN);
      ClearTimer(fd);
    }
  }
}

void Writer(Coroutine* c) {
  for (int i = 0; i < 20; i++) {
    char buf[256];
    size_t n = snprintf(buf, sizeof(buf), "FOO %d\n", i);
    CoroutineWait(c, pipes[1], POLLOUT);
    write(pipes[1], buf, n);
    CoroutineYield(c);
  }
  close(pipes[1]);
}

void Reader(Coroutine* c) {
  for (;;) {
    char buf[256];
    CoroutineWait(c, pipes[0], POLLIN);
    ssize_t n = read(pipes[0], buf, sizeof(buf));
    if (n == 0) {
      printf("EOF\n");
      break;
    }
    buf[n] = '\0';
    printf("Received: %s", buf);
  }
  close(pipes[0]);
}

int main(int argc, const char* argv[]) {
  pipe(pipes);

  CoroutineMachine m;
  CoroutineMachineInit(&m);
  Coroutine* c1 = NewCoroutine(&m, Co1);

  CoroutineStart(c1);

  Coroutine* writer = NewCoroutine(&m, Writer);
  Coroutine* reader = NewCoroutine(&m, Reader);

  CoroutineStart(reader);
  CoroutineStart(writer);

  CoroutineMachineRun(&m);

  CoroutineMachineDestruct(&m);
}
