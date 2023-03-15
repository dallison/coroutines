//
//  coroutine.c
//  coroutines
//
//  Created by David Allison on 3/13/23.
//

#include "coroutine.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bitset.h"

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#elif defined(__linux__)
#include <sys/eventfd.h>

#else
#error "Unknown operating system"
#endif

#if defined(__aarch64__)
#define GET_STACK_POINTER() \
  void* sp; \
  asm("mov %0, sp" : "=r" (sp));
#endif


static int NewEventFd() {
  int event_fd;
#if defined(__APPLE__)
  event_fd = kqueue();
#elif defined(__linux__)
  event_fd = eventfd(0, EFD_NONBLOCK);
#else
#error "Unknown operating system"
#endif
  return event_fd;
}

static void CloseEventFd(int fd) {
  if (fd == -1) {
    return;
  }
  close(fd);
}

static void TriggerEvent(int fd) {
#if defined(__APPLE__)
  struct kevent e;
  EV_SET(&e, 1, EVFILT_USER, EV_ADD, NOTE_TRIGGER, 0, NULL);
  kevent(fd, &e, 1, 0, 0, 0); // Trigger USER event
#elif defined(__linux__)
  int64_t val = 1;
  (void)write(fd, &val, 8);
#else
#error "Unknown operating system"
#endif
}

static void ClearEvent(int fd) {
#if defined(__APPLE__)
  struct kevent e;
  EV_SET(&e, 1, EVFILT_USER, EV_DELETE, NOTE_TRIGGER, 0, NULL);
  kevent(fd, &e, 1, NULL, 0, 0); // Clear USER event
#elif defined(__linux__)
  int64_t val;
  (void)read(fd, &val, 8);
#else
#error "Unknown operating system"
#endif
  struct pollfd f = {.fd = fd, .events = POLLIN};
  int x = poll(&f, 1, 0);
}

void CoroutineInit(Coroutine* c,struct CoroutineMachine* machine,
                   CoroutineFunctor functor) {
  CoroutineInitWithStackSize(c, machine, functor, kCoDefaultStackSize);
}

void CoroutineInitWithStackSize(Coroutine* c,struct CoroutineMachine* machine,
                                CoroutineFunctor functor, size_t stack_size) {
  c->functor = functor;
  c->stack_size = stack_size;
  c->state = kCoNew;
  c->machine = machine;
  c->stack = malloc(stack_size);
  ListElementInit(&c->element);
  c->event_fd.fd = NewEventFd();
  c->event_fd.events = POLLIN;

  c->wait_fd.fd = -1;
  c->wait_fd.events = POLLIN;

  c->caller = NULL;
  c->result = NULL;
  c->result_size = 0;
  c->user_data = NULL;
  
  // Add to machine but do not start it.
  CoroutineMachineAddCoroutine(machine, c);
}

Coroutine* NewCoroutine(CoroutineMachine* machine,
                                     CoroutineFunctor functor) {
  return NewCoroutineWithStackSize(machine, functor, kCoDefaultStackSize);
}

Coroutine* NewCoroutineWithStackSize(CoroutineMachine* machine,
                        CoroutineFunctor functor, size_t stack_size) {
  Coroutine* c = malloc(sizeof(Coroutine));
  CoroutineInitWithStackSize(c, machine, functor, stack_size);
  return c;
}

void CoroutineDestruct(Coroutine* c) {
  free(c->stack);
  CloseEventFd(c->event_fd.fd);
  CloseEventFd(c->wait_fd.fd);
}

void CoroutineDelete(Coroutine* c) {
  CoroutineDestruct(c);
  free(c);
}

void CoroutineExit(Coroutine* c) {
  longjmp(c->exit, 1);
}

void CoroutineStart(Coroutine* c) {
  if (c->state == kCoNew) {
    c->state = kCoReady;
  }
}



void CoroutineWait(Coroutine* c, int fd, int event_mask) {
  c->state = kCoWaiting;
  c->wait_fd.fd = fd;
  c->wait_fd.events = event_mask;
  if (setjmp(c->resume) == 0) {
    longjmp(c->machine->yield, 1);
  }
}

void CoroutineTriggerEvent(Coroutine* c) {
  TriggerEvent(c->event_fd.fd);
}

void CoroutineClearEvent(Coroutine* c) {
  ClearEvent(c->event_fd.fd);
}

static struct pollfd* GetPollFd(Coroutine* c) {
  static struct pollfd empty = {.fd = -1, .events = 0, .revents = 0};
  switch (c->state) {
    case kCoReady:
    case kCoYielded:
      return &c->event_fd;
    case kCoWaiting:
      return &c->wait_fd;
    case kCoNew:
    case kCoRunning:
    case kCoDead:
      return &empty;
  }
}

bool CoroutineIsAlive(Coroutine* c) {
  return c->state != kCoDead;
}

void CoroutineYield(Coroutine* c) {
  c->state = kCoYielded;
  if (setjmp(c->resume) == 0) {
    CoroutineTriggerEvent(c);
    longjmp(c->machine->yield, 1);
    // Never get here.
  }
  // We get here when resumed.
}

void CoroutineYieldValue(Coroutine* c, void* value) {
  // Copy value.
  if (c->result != NULL) {
    memcpy(c->result, value, c->result_size);
  }
  if (c->caller != NULL) {
    // Tell caller that there's a value available.
    CoroutineTriggerEvent(c->caller);
  }
  
  // Yield control to another coroutine but don't trigger a wakup event.
  // This will be done when another call is made.
  c->state = kCoYielded;
  if (setjmp(c->resume) == 0) {
    longjmp(c->machine->yield, 1);
    // Never get here.
  }
  // We get here when resumed from another call.
}

void CoroutineCall(Coroutine* c, Coroutine* callee, void* result, size_t result_size) {
  // Tell the callee that it's being called and where to store the value.
  callee->caller = c;
  callee->result = result;
  callee->result_size = result_size;
  
  // Start the callee running if it's not already running.  If it's running
  // we trigger its event to wake it up.
  if (callee->state == kCoNew) {
    CoroutineStart(callee);
  } else {
    CoroutineTriggerEvent(callee);
  }
  c->state = kCoYielded;
  if (setjmp(c->resume) == 0) {
    longjmp(c->machine->yield, 1);
    // Never get here.
  }
  // When we get here, the callee has done its work.  Remove this coroutine's
  // state from it.
  callee->caller = NULL;
  callee->result = NULL;
}

// This invokes the coroutine's functor and long jumps to the exit environment.
// The coroutine's stack pointer is set to the allocated stack and restored
// aferwards.
static void SwitchStackAndRun(void* sp, CoroutineFunctor f, void* arg, jmp_buf exit) {
#if defined(__aarch64__)
  asm(
      "mov x12, sp\n"     // Save current stack pointer.
      "mov x13, x29\n"    // Save current frame pointer
      "mov x29, #0\n"     // FP = 0
      "sub sp, %0, #32\n"      // Set new stack pointer.
      "stp x12, x13, [sp, #16]\n"
      "str %3, [sp]\n"
      "mov x0, %2\n"      // Load arg to functor
      "blr %1\n"          // Invoke functor on new stack
      "ldr x0, [sp]\n"
      "ldp x12, x29, [sp, #16]\n"
      "mov sp, x12\n"     // Restore stack pointer
      "mov w1, #1\n"
#if defined(__APPLE__)
      "bl _longjmp\n"
#else
      "bl longjmp\n"
#endif
      : /* no output regs*/
      : "r" (sp), "r" (f), "r" (arg), "r" (exit)
      );
#elif defined(__x86_64__)
  asm(
      "movq %%rsp, %%r14\n"     // Save current stack pointer.
      "movq %%rbp, %%r15\n"    // Save current frame pointer
      "movq $0, %%rbp\n"     // FP = 0
      "movq %0, %%rsp\n"
      "pushq %%r14\n"		// Push rsp
      "pushq %%r15\n"		// Push rbp
      "pushq %3\n"		// Push env
      "subq $8, %%rsp\n"	// Align to 16
      "movq %2, %%rdi\n"
      "callq *%1\n"
      "addq $8, %%rsp\n"	// Remove alignment.
      "popq %%rdi\n"		// Pop env
      "popq %%rbp\n"
      "popq %%rsp\n"
      "movl $1, %%esi\n"
      "callq longjmp\n"
      : /* no output regs*/
      : "r" (sp), "r" (f), "r" (arg), "r" (exit)
      );
#else
#error "Unknown architecture"
#endif
}

static void Resume(Coroutine* c) {
  switch (c->state) {
    case kCoReady:
      c->state = kCoRunning;
      if (setjmp(c->exit) == 0) {
        SwitchStackAndRun(c->stack + c->stack_size, c->functor, c, c->exit);
      }
      // Trigger the caller when we exit.
      if (c->caller != NULL) {
        CoroutineTriggerEvent(c->caller);
      }
      // Functor returned, we are dead.
      c->state = kCoDead;
      CoroutineMachineRemoveCoroutine(c->machine, c);
      break;
    case kCoYielded:
    case kCoWaiting:
    case kCoDead:
      c->state = kCoRunning;
      longjmp(c->resume, 1);
      break;
    case kCoRunning:
    case kCoNew:
      // Should never get here.
      break;
  }
}

void CoroutineSetUserData(Coroutine* c, void* user_data) {
  c->user_data = user_data;
}

void* CoroutineGetUserData(Coroutine* c) {
  return c->user_data;
}

void CoroutineMachineInit(CoroutineMachine* m) {
  ListInit(&m->coroutines);
  m->current = NULL;
  m->running = false;
  m->pollfds = NULL;
  m->pollfd_capacity = 0;
  m->num_pollfds = 0;
  VectorInit(&m->blocked_coroutines);
  m->interrupt_fd.fd = NewEventFd();
  m->interrupt_fd.events = POLLIN;
}


static void AddPollFd(CoroutineMachine* m, struct pollfd* fd) {
  if (m->num_pollfds >= m->pollfd_capacity) {
    nfds_t new_capacity = m->pollfd_capacity == 0 ? 2 : m->pollfd_capacity * 2;
    m->pollfds = realloc(m->pollfds, new_capacity * sizeof(*fd));
    m->pollfd_capacity = new_capacity;
  }
  m->pollfds[m->num_pollfds] = *fd;
  m->num_pollfds++;
}



static Coroutine* GetRunnableCoroutine(CoroutineMachine* m) {
  m->num_pollfds = 0;
  AddPollFd(m, &m->interrupt_fd);
  VectorClear(&m->blocked_coroutines);
  for (ListElement* e = m->coroutines.first; e != NULL; e = e->next) {
    Coroutine* c = (Coroutine*)e;
    if (c->state == kCoNew ||
      c->state == kCoRunning || c->state == kCoDead) {
      continue;
    }
    struct pollfd* fd = GetPollFd(c);
    AddPollFd(m, fd);
    VectorAppend(&m->blocked_coroutines, c);
    if (c->state == kCoReady) {
      // Coroutine is ready to go, trigger its event so that we can start
      // it.
      CoroutineTriggerEvent(c);
    }
  }
  
  // Wait for coroutines (or the interrupt fd) to trigger.
  int e = poll(m->pollfds, m->num_pollfds, -1);
  if (e == 0) {
    return NULL;
  }
  
  if (m->interrupt_fd.revents != 0) {
    // Interrupted.
    ClearEvent(m->interrupt_fd.fd);
  }
  if (!m->running) {
    // If we have been asked to stop, there's nothing else to do.
    return NULL;
  }
  BitSet runnables = {0};
  // We have a number of pollfds ready.  Pick one fairly.
  for (size_t i = 1; i < m->num_pollfds; i++) {
    struct pollfd* fd = &m->pollfds[i];
    if (fd->revents != 0) {
      if ((fd->revents & POLLHUP) != 0) {
        // Hangup means we have a closed file descriptor.  Coroutine is done.
        Coroutine* c = m->blocked_coroutines.value.p[i-1];
        c->state = kCoDead;
      }
      BitSetInsert(&runnables, i-1);
    }
  }
  
  // Pick a random runnable coroutine.  If there is more than one runnable,
  // we avoid choosing the one that last yielded.
  BitSetIterator it;
  size_t num_runnables = BitSetCount(&runnables);
  Coroutine* chosen = NULL;
  bool done = false;
  while (!done) {
    int j = rand() % num_runnables;
    int iteration = 0;
    BitSetIteratorStart(&it, &runnables);
    done = true;
    while (!BitSetIteratorDone(&it)) {
      if (iteration == j) {
        chosen = m->blocked_coroutines.value.p[BitSetIteratorValue(&it)];
        if (num_runnables != 1 && chosen == m->current &&
            chosen->state == kCoYielded) {
          // Don't choose coroutine that just yielded if there is
          // another one.
          done = false;
          chosen = NULL;
        }
        break;
      }
      BitSetIteratorNext(&it);
      iteration++;
    }
  }
  BitSetDestruct(&runnables);
  if (chosen != NULL) {
    CoroutineClearEvent(chosen);
  }
  return chosen;
}

void CoroutineMachineDestruct(CoroutineMachine* m) {
  ListDestruct(&m->coroutines);
  CloseEventFd(m->interrupt_fd.fd);
}

void CoroutineMachineRun(CoroutineMachine* m) {
  m->running = true;
  while (m->running) {
    if (m->coroutines.length == 0) {
      // No coroutines, nothing to do.
      break;
    }
    setjmp(m->yield);
    // We get here any time a coroutine yields or waits.
    
    Coroutine* c = GetRunnableCoroutine(m);
    m->current = c;
    if (c != NULL) {
      Resume(c);
    }
  }
}

void CoroutineMachineAddCoroutine(CoroutineMachine* m, Coroutine* c) {
  ListAppend(&m->coroutines, &c->element);
}

// Removes a coroutine but doesn't free it.
void CoroutineMachineRemoveCoroutine(CoroutineMachine* m, Coroutine* c) {
  for (ListElement* e = m->coroutines.first; e != NULL; e = e->next) {
    Coroutine* co = (Coroutine*)e;
    if (co == c) {
      ListDeleteElement(&m->coroutines, e);
      return;
    }
  }
}

void CoroutineMachineStop(CoroutineMachine* m) {
  m->running = false;
  TriggerEvent(m->interrupt_fd.fd);
}
