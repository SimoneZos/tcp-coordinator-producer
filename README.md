# TCP Coordinator & Producer (Inter-Process Communication)

A client-server architecture developed in C demonstrating advanced Operating Systems concepts, including TCP socket networking, multi-process concurrency, file locking mechanisms, and asynchronous signal handling.

## ⚙️ Core Technical Features

* **TCP Client-Server Communication**: Reliable byte-stream data transfer using standard POSIX sockets (`bind`, `listen`, `accept`, `connect`).
* **Multi-Process Concurrency**: The Coordinator uses `fork()` to spawn dedicated child processes for handling incoming Producer connections simultaneously without blocking the main server loop.
* **Synchronization & Safety**: Implements `fcntl` file locks (`F_WRLCK`) to prevent race conditions when multiple child processes write to the shared `log.txt` file concurrently.
* **Advanced Signal Handling**:
  * `SIGINT`: Ensures a graceful shutdown by waiting for all child processes to finish execution before terminating.
  * `SIGALRM`: Acts as a daemon timer to monitor the log size, automatically rotating `log.txt` to `vecchio_log.txt` if it exceeds 1024 bytes.
  * `SIGPIPE`: Custom logic to gracefully catch unexpected client disconnections.

## 🚀 How to Build and Run

### Compilation
Open a terminal and compile both source files using `gcc`:

```bash
gcc -Wall -Wextra -o coordinatore coordinatore.c
gcc -Wall -Wextra -o produttore produttore.c
```

### Execution

1. Start the Coordinator (Server) in the first terminal:
```bash
./coordinatore
```

2. Open a second terminal window and run the Producer (Client). You can pass an optional integer argument:
```bash
./produttore 99
```
*(If no argument is passed, it defaults to sending the number 42).*

You can run the Producer multiple times or from multiple terminals simultaneously to test the Coordinator's concurrency and file-locking capabilities.
