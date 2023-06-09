# coroutines
Coroutines in C

This is a simple prototype coroutines library written in C.

## What is a coroutine?
A coroutine is a cooperative multitasking object.  It is basically a function
that executes in parallel with other functions in the single threaded
program.  A coroutine is never preempted by the kernel in order to give
another coroutine the CPU.  All context switches between coroutines are
voluntary.

## Why coroutines?
Because multithreading is almost impossible to get right and the cause,
in my opinion, of the vast majority of the bugs in today's software. 
Coroutines allow programs to be single threaded while also making use of
blocking file descriptors.  Without coroutines, you would need to use
non-blocking file descriptors to avoid threading and those introduce even
more complexity.

## How this works
There is a *CoroutineMachine* object that is responsible for running all
the coroutines.  It owns all the coroutines and enables them to yield to
one another and wait for I/O.  It is the main loop and it is expected that
it will be run from the *main* function in the program.

Each coroutine's state is held in a *Coroutine* object that is created when
necessary, usually by another coroutine.  It is very cheap to create a
coroutine.  Each *Coroutine* object is passed a pointer to a function that
will be the body of the coroutine.  This function is called when the coroutine
is started and runs until completion, at which point the coroutine object
ceases to exist.

The coroutines function, referred to as its *functor* is a function with the
following signature:

```
void (*)(Coroutine*);
```

That is, it takes a single pointer to the *Coroutine* object and returns void.

## Constructing a coroutine
A *Coroutine* object is generally constructed by another coroutine and it
may be built either on the caller's stack or from heap memory.  The
following functions exist to create a *Coroutine* object:

```
// Initialize a coroutine with the default stack size.
void CoroutineInit(Coroutine* c, struct CoroutineMachine* machine,
                   CoroutineFunctor functor);

// Initialize a coroutine with given stack size.
void CoroutineInitWithStackSize(Coroutine* c, struct CoroutineMachine* machine,
                                CoroutineFunctor functor, size_t stack_size);

void CoroutineInitWithUserData(Coroutine* c, struct CoroutineMachine* machine,
                               CoroutineFunctor functor, void* user_data);
void CoroutineInitWithStackSizeAndUserData(Coroutine* c,
                                           struct CoroutineMachine* machine,
                                           CoroutineFunctor functor,
                                           size_t stack_size, void* user_data);

// Allocate new coroutine on heap with default stack size.
Coroutine* NewCoroutine(struct CoroutineMachine* machine,
                        CoroutineFunctor functor);
Coroutine* NewCoroutineWithStackSize(struct CoroutineMachine* machine,
                                     CoroutineFunctor functor,
                                     size_t stack_size);

Coroutine* NewCoroutineWithUserData(struct CoroutineMachine* machine,
                                    CoroutineFunctor functor, void* user_data);
Coroutine* NewCoroutineWithStackSizeAndUserData(
    struct CoroutineMachine* machine, CoroutineFunctor functor,
    size_t stack_size, void* user_data);
```

The functions starting with *NewCoroutine* allocate the *Coroutine* from the
heap and return a pointer to it.  The function starting with *CoroutineInit*
take a pointer to a *Coroutine* object and initialize it.

The corresponding functions to destruct and delete the *Coroutine* are:

```
void CoroutineDestruct(Coroutine* c);
void CoroutineDelete(Coroutine* c);
```

The former destructs a *Coroutine* without freeing the memory, while the
latter calls *free* on the pointer.  Obviously it's important to call the
right one, but these are rarely needed to be called by the user as coroutines
are self-destructing.

Each coroutine has a unique integral ID, managed by the *CoroutineMachine*.  The
IDs are held in a compressed bitset and will be reused aggressively.  In other
words, once an ID is freed up, it will be taken by another coroutine.

You can ask is a coroutine is alive by calling *CoroutineIsAlive*.

Coroutines each have their own runtime stack, allocated from the heap
when the *Coroutine* object is constructed.  By default this stack is pretty
small at only **8KB**.  The should be sufficient for most small tasks, but
you have full control over the amount of memory allocated for a stack as
a parameter to the construction functions.

Coroutines also have some user data that can be provided by the caller.  This
is generally a pointer to some memory that holds arguments passed to the
coroutine.  It can be passed when the coroutine is constructed or by calling

```
void CoroutineSetUserData(Coroutine* c, void* user_data);
```

The coroutine can retrieve the user data by accessing the *user_data*
member of the *Coroutine* object or by calling:

```
void* CoroutineGetUserData(Coroutine* c);
```

Coroutines all have a name, which is generated by default to be **co-N**
where N is the routine's ID.  You can set this name to something else by
calling:

```
void CoroutineSetName(Coroutine* c, const char* name);
```

And you can get it by calling:

```
const char* CoroutineGetName(Coroutine* c);
```


## Starting a coroutine
When a *Coroutine* is constructed it is not yet ready to run.  To put it in
a state where it will be invoked, you must started it by calling
**CoroutineStart**.  When this is called, the *CoroutineMachine* will schedule
it for running and it will be invoked at the earliest opportunity.

## Cooperating with other coroutines
This is the most important aspect of coroutines - you must not hog the CPU or
block it and not allow others to run.  The main way to break a program using
coroutines is by calling a blocking I/O function, like *read* on a file 
descriptor that has no data availalble.  Therefore it is very important that 
before a coroutine calls anything that might block the program for any 
significant amount of time, the coroutine must yield control and allow 
others to run.

There are two ways to yield control:

1. Calling *CoroutineYield*
1. Calling *CoroutineWait*

The former can be used when the coroutine is processing something that
will take a long time and wants to give another coroutine a chance to run.

The latter is used before the coroutine wants to perform I/O on a file
descriptor that could block the program.

Both of these functions yield control to the *CoroutineMachine* which will
choose another coroutine to run.

For example, if the coroutine wants to read from a file descriptor, you
must do something like this:

```
CoroutineWait(c, fd, POLLIN);
ssize_t n = read(fd, buf, sizeof(buf));
```

The *CoroutineWait* function yields control from the coroutine and the
coroutine will resume execution when the given file descriptor has the
event *POLLIN* available in the *CoroutineMachine*'s call to *poll* (this uses
multiplexed I/O).

Because this uses multiplexed I/O a file descriptor of any type may be used
and not just sockets or files.  For example you could use a *timerfd* on Linux
to create a timed wait.

A possible enhancement would be to allow a wait to be performed on multiple
file descriptors.

## The Main Loop
The main loop looks something like this:

```
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
```

The *Listener* is a coroutine functor that is run by the machine.  It can
create other coroutines as needed.

## Scheduling
This library uses a round-robin fair scheduling algorithm that always chooses
the coroutine that has been waiting the longest.  There is no provision for
priority.

## Examples
Two reasonably functional examples are provided for your enjoyment:

1. A simple HTTP server
1. An HTTP client

The server only handles GET requests for files on the local machine.  It is
single-thread but uses coroutines to execute all requests simulataneously.
It is very efficient.

The client is meant to exercise the server and allows multiple GET requests
to be sent to a server at the same time.  It just gets a file and prints it
to standard output.  All output is interleaved.  It is single threaded
and uses coroutines to perform each request.  Be careful doing too many
of them at once because you will run out of file descriptors (MacOS sets a 
limit of 256 in the shell, but you change it).

## Generators and inter-coroutine calls
A pretty cool use of coroutines is to allow them to be used as a *Generator*
where one coroutine calls another, which generates a value and yields back
control to the caller.  Then, when another call is made, a new value is
generated, etc.

This is supported by this implementation using the following functions:

1. CoroutineCall
1. CoroutineYieldValue

The former yields control of the CPU and invokes or resumes another coroutine
passing it a location into which a value may be stored.

The latter is used in the generator to yield control back to the caller
and return a value to it.

Here are the function signatures:

```
void CoroutineCall(Coroutine* c, Coroutine* callee, void* result,
                   size_t result_size);
void CoroutineYieldValue(Coroutine* c, void* value);                   
```

The caller coroutine passes the address and size of the value it would
like from the callee coroutine.  You can pass NULL for no value.
The generator coroutine is uses the *CoroutineYieldValue* to yield back
and copy the value (of the appropriate size) into the address provided
by the caller.

Here's an example:

```
void Generator(Coroutine* c) {
  for (int i = 1; i < 50; i++) {
    CoroutineYieldValue(c, &i);
  }
}

void Numbers(Coroutine* c) {
  Coroutine generator;
  CoroutineInit(&generator, c->machine, Generator);
  while (CoroutineIsAlive(c, &generator)) {
    int value = 0;
    CoroutineCall(c, &generator, &value, sizeof(value));
    if (CoroutineIsAlive(c, &generator)) {
      printf("Value: %d\n", value);
    }
  }
}
```


