# Publish–Subscribe Chat System (C, TCP, pthreads)

This project implements a **TCP-based publish–subscribe messaging system** written in C.  
It consists of a **server**, **publishers**, and **subscribers**, allowing publishers to publish news to topics and subscribers to receive messages for topics they are subscribed to.

---

## Features

* TCP client–server architecture
* Multiple concurrent clients using **POSIX threads**
* Topic-based publish–subscribe model
* Dynamic topic creation
* Multiple subscribers per topic
* Safe concurrent access using **mutexes**
* Separate publisher and subscriber clients
* Graceful client disconnect support
* Server IP and port configurable via command-line arguments
* Graceful server shutdown with **Ctrl+C**
* Publisher monitors server connection using a dedicated thread
* Automatic build and demo run using **Makefile**

---

## Architecture Overview

### Server

* Accepts TCP connections
* Distinguishes clients as **PUBLISHER** or **SUBSCRIBER**
* Maintains a global **topic registry**
* Forwards published messages to all subscribers of a topic
* Can be terminated gracefully using **Ctrl+C**

---

### Publisher Client

* Publishes messages to topics
* Automatically creates a topic if it does not exist
* Connects to the server using command-line provided IP and port
* Uses a **separate monitoring thread** to detect server disconnection

The connection state is tracked using a shared flag:

```c
bool server_disconnected = false;
pthread_mutex_t server_disconnected_mutex = PTHREAD_MUTEX_INITIALIZER;
```

The monitoring thread:

* listens for socket/connection errors
* sets `server_disconnected = true` when the server disconnects
* safely synchronizes access using the mutex
* allows the main thread to terminate gracefully without blocking

---

### Subscriber Client

* Subscribes/unsubscribes to topics
* Receives messages asynchronously from the server
* Connects to the server using command-line provided IP and port

---

## Project Structure

```
.
├── server.c          # Chat server
├── publisher.c       # Publisher client
├── subscriber.c      # Subscriber client
├── list.c            # Topic & subscriber linked-list logic
├── list.h            # Data structures and function declarations
├── Makefile
└── README.md
```

---

## Data Structures

### Topic

```c
typedef struct topic {
    char *name;
    SUBSCRIBER *subscribers;
    struct topic *nextTopic;
} TOPIC;
```

### Subscriber

```c
typedef struct subscriber {
    int socket;
    struct subscriber *next;
} SUBSCRIBER;
```

---

## Compilation

Use `gcc` with pthread support.

### Server

```bash
gcc server.c list.c -o server -pthread
```

### Publisher

```bash
gcc publisher.c -o publisher -pthread
```

### Subscriber

```bash
gcc subscriber.c -o subscriber -pthread
```

---

# Running the Application

## 1. Start the Server

```bash
./server
```

Server listens on:

```
127.0.0.1:12345
```

---

## 2. Start a Subscriber

```bash
./subscriber <server_ip> <server_port>
```

Example:

```bash
./subscriber 127.0.0.1 12345
```

### Subscriber Commands

```text
/subscribe "topic1" "topic2"
/unsubscribe "topic1" "topic2"
/topics
/exit
```

---

## 3. Start a Publisher

```bash
./publisher <server_ip> <server_port>
```

Example:

```bash
./publisher 127.0.0.1 12345
```

### Publish Message Format

```text
[topic] "message text"
```

Example:

```text
[sports] "Team A won the match"
```

---

# Server Shutdown

The server can be stopped at any time using:

```bash
Ctrl + C
```

When the server shuts down:

* all client sockets are closed
* subscribers and publishers detect the disconnect
* their receive/monitor threads terminate

## Important

After the server exits, **subscriber and publisher clients must press ENTER once** to close.

This is required because their input thread may still be waiting for keyboard input (`stdin`).

Without pressing ENTER, the program may appear stuck.

---

## Message Flow

1. Publisher sends:

   ```
   [news] "Breaking news!"
   ```

2. Server:
   * Extracts topic name
   * Creates topic if it does not exist
   * Broadcasts the message to all subscribers

3. Subscribers receive:

   ```
   [news] "Breaking news!"
   ```

---

## Concurrency & Synchronization

### Server

The **topic registry** is protected using:

```c
pthread_mutex_t topicRegistry_mtx;
```

Prevents race conditions during:

* Topic creation
* Subscription changes
* Message broadcasting

---

### Subscriber

Client shutdown is synchronized using:

```c
bool exit_flag;
pthread_mutex_t exit_mutex;
```

Ensures coordinated and graceful termination of threads.

---

### Publisher

Server connection monitoring:

```c
bool server_disconnected = false;
pthread_mutex_t server_disconnected_mutex = PTHREAD_MUTEX_INITIALIZER;
```

Used by a dedicated thread to safely notify the main thread about connection loss.

---

# Makefile Usage

A **Makefile** is provided to simplify building and testing.

## Build everything

```bash
make
```

Compiles:

* server
* publisher
* subscriber

---

## Run demo automatically

```bash
make run
```

This automatically starts:

* 1 server
* 2 subscribers
* 2 publishers

Each runs in its own terminal window.

Useful for quickly testing:

* concurrent clients
* subscriptions
* live message broadcasting

---

## Clean binaries

```bash
make clean
```

Removes compiled executables.

---

## Error Handling

* Invalid message formats are rejected
* Duplicate subscriptions are ignored
* Invalid arguments terminate the client
* Memory allocation failures are checked
* Socket errors are reported using `perror`

---

## Cleanup

* Subscribers are removed from all topics on disconnect
* All allocated memory is properly freed

---

## License

This project is intended for **educational purposes**.  
You are free to modify and extend it.
