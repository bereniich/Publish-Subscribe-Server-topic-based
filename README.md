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

---

## Architecture Overview

### Server

* Accepts TCP connections
* Distinguishes clients as **PUBLISHER** or **SUBSCRIBER**
* Maintains a global **topic registry**
* Forwards published messages to all subscribers of a topic

### Publisher Client

* Publishes messages to topics
* Automatically creates a topic if it does not exist
* Connects to the server using command-line provided IP and port

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
└── README.md

````

---

## Data Structures

### Topic

```c
typedef struct topic {
    char *name;
    SUBSCRIBER *subscribers;
    struct topic *nextTopic;
} TOPIC;
````

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
gcc publisher.c -o publisher
```

### Subscriber

```bash
gcc subscriber.c -o subscriber -pthread
```

---

## Running the Application

### 1. Start the Server

```bash
./server
```

Server listens on:

```
127.0.0.1:12345
```

---

### 2. Start a Subscriber

```bash
./subscriber <server_ip> <server_port>
```

Example:

```bash
./subscriber 127.0.0.1 12345
```

You will receive a list of available topics upon connection.

#### Subscriber Commands

```text
[SUBSCRIBE] topic1 topic2
[UNSUBSCRIBE] topic1
/exit
```

---

### 3. Start a Publisher

```bash
./publisher <server_ip> <server_port>
```

Example:

```bash
./publisher 127.0.0.1 12345
```

#### Publish Message Format

```text
[topic] "message text"
```

Example:

```text
[sports] "Team A won the match"
```

---

## Message Flow

1. Publisher sends:

   ```
   [news] "Breaking news!"
   ```
2. Server:

   * Extracts topic name
   * Creates topic if it does not exist
   * Sends the message to all subscribers of `news`
3. Subscribers receive:

   ```
   [news] "Breaking news!"
   ```

---

## Concurrency & Synchronization

* The **topic registry** is protected using:

```c
pthread_mutex_t topicRegistry_mtx;
```

* Prevents race conditions during:

  * Topic creation
  * Subscription changes
  * Message broadcasting

---

## Error Handling

* Invalid message formats are rejected
* Duplicate subscriptions are ignored
* Invalid command-line arguments terminate the client
* Memory allocation failures are checked
* Socket errors are reported using `perror`

---

## Cleanup

* Subscriber removal from all topics on disconnect
* Proper memory deallocation for:

  * Topics
  * Subscribers
  * Client structures

---

## Notes

* Server IP address and port are no longer hardcoded in the clients.
* Valid port range: `1–65535`
* Incorrect argument count displays usage instructions.

---

## License

This project is intended for **educational purposes**.
You are free to modify and extend it.


