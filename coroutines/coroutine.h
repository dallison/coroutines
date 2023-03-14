//
//  coroutine.h
//  coroutines
//
//  Created by David Allison on 3/13/23.
//

#ifndef coroutine_h
#define coroutine_h

#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include "list.h"
#include "vector.h"
#include <poll.h>

struct CoroutineMachine;
struct Coroutine;

typedef void (*CoroutineFunctor)(struct Coroutine* c);
#define kCoDefaultStackSize 8192

typedef enum  {
  kCoNew,
  kCoReady,
  kCoRunning,
  kCoYielded,
  kCoWaiting,
  kCoDead,
} CoroutineState;

// This is a Coroutine.  It executes its functor (pointer to a function).
// It has its own stack.
typedef struct Coroutine {
  ListElement element;
  CoroutineFunctor functor;     // Coroutine body.
  CoroutineState state;
  void* stack;                  // Stack, allocated from malloc.
  size_t stack_size;
  jmp_buf resume;               // Program environemnt for resuming.
  jmp_buf exit;                 // Program environemt to exit.
  struct pollfd event_fd;       // Pollfd for event.
  struct pollfd wait_fd;        // Pollfd for waiting for an fd.
  struct CoroutineMachine* machine;
  struct Coroutine* caller;     // If being called, who is calling us.
  void* result;                 // Where to put result in YieldValue.
  size_t result_size;           // Length of value to store.
  void* user_data;              // User data, not owned by this.
} Coroutine;

// Initialize a coroutine with the default stack size.
void CoroutineInit(Coroutine* c,struct CoroutineMachine* machine,
                   CoroutineFunctor functor);

// Initialize a coroutine with given stack size.
void CoroutineInitWithStackSize(Coroutine* c,struct CoroutineMachine* machine,
                   CoroutineFunctor functor, size_t stack_size);

// Allocate new coroutine on heap with default stack size.
Coroutine* NewCoroutine(struct CoroutineMachine* machine,
                        CoroutineFunctor functor);
Coroutine* NewCoroutineWithStackSize(struct CoroutineMachine* machine, CoroutineFunctor functor, size_t stack_size);

// Destruct a coroutine.
void CoroutineDestruct(Coroutine* c);
void CoroutineDelete(Coroutine* c);

// Start a coroutine running if it is not already running,
void CoroutineStart(Coroutine* c);

// Yield control to another coroutine.
void CoroutineYield(Coroutine* c);

// Yield control and store value.
void CoroutineYieldValue(Coroutine* c, void* value);

// Wait for a file descriptor to become ready.
void CoroutineWait(Coroutine* c, int fd, int event_mask);

void CoroutineTriggerEvent(Coroutine* c);
void CoroutineClearEvent(Coroutine* c);
void CoroutineExit(Coroutine* c);

void CoroutineSetUserData(Coroutine* c, void* user_data);
void* CoroutineGetUserData(Coroutine* c);

void CoroutineCall(Coroutine* c, Coroutine* callee, void* result, size_t result_size);
bool CoroutineIsAlive(Coroutine* c);

typedef struct CoroutineMachine {
  List coroutines;
  Coroutine* current;
  jmp_buf yield;
  bool running;
  struct pollfd* pollfds;
  nfds_t pollfd_capacity;
  nfds_t num_pollfds;
  Vector blocked_coroutines;
  struct pollfd interrupt_fd;
} CoroutineMachine;

void CoroutineMachineInit(CoroutineMachine* m);
CoroutineMachine* NewCoroutineMachine(void);
void CoroutineMachineDestruct(CoroutineMachine* m);
void CoroutineMachineDelete(CoroutineMachine* m);
void CoroutineMachineStop(CoroutineMachine* m);

void CoroutineMachineAddCoroutine(CoroutineMachine* m, Coroutine* c);
void CoroutineMachineRemoveCoroutine(CoroutineMachine* m, Coroutine* c);
void CoroutineMachineStartCoroutine(Coroutine* c);

void CoroutineMachineRun(CoroutineMachine* m);


#endif /* coroutine_h */
