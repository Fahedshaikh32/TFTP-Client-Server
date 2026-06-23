# TFTP Client-Server | RFC 1350 Implementation in C

A production-style implementation of the Trivial File Transfer Protocol (TFTP) based on RFC 1350, developed in C using UDP sockets on Linux.

This project focuses on low-level network programming, protocol implementation, reliability mechanisms, and concurrent request handling. The implementation supports both client and server operations, multiple transfer modes, retransmission on packet loss, timeout management, and dynamic client session allocation.

---

## Overview

TFTP is a lightweight file transfer protocol that operates over UDP. Unlike TCP-based protocols, reliability must be implemented at the application layer through acknowledgements, retransmissions, block sequencing, and timeout management.

This project implements:

* Read Request (RRQ)
* Write Request (WRQ)
* DATA packets
* ACK packets
* ERROR packets
* Client-server communication over UDP
* Timeout and retransmission handling
* Multi-client support using process isolation
* Dynamic session port allocation
* Multiple transfer modes

---

## Key Engineering Features

### RFC 1350 Protocol Compliance

The implementation follows the core TFTP packet exchange model defined by RFC 1350:

RRQ / WRQ → ACK → DATA → ACK → DATA → ACK

including:

* Block-based file transfer
* End-of-transfer detection
* Error packet handling
* Session-specific communication

---

### Reliable Transfer over UDP

Since UDP provides no delivery guarantees, reliability is implemented at the application layer.

Implemented mechanisms:

* Receive timeout detection
* Automatic packet retransmission
* Retry limits
* Transfer abort on repeated failures
* Acknowledgement validation

This allows successful file transfer despite packet loss scenarios.

---

### Concurrent Client Handling

The server supports multiple client sessions through process-based concurrency.

Design:

* Parent process listens on well-known port 6969
* Each incoming request is assigned a dedicated transfer port
* A child process is created using fork()
* Client communication continues independently on the allocated port

Benefits:

* Session isolation
* Parallel transfers
* Simplified resource management
* Improved scalability compared to a single-threaded design

---

### Dynamic Port Allocation

Instead of handling all transfers on the listening socket, the server allocates unique ports for individual transfer sessions.

Example:

Client A → Port 20000

Client B → Port 20001

Client C → Port 20002

This mirrors how many real-world protocol servers separate control and data communication.

---

### Transfer Modes

#### Normal Mode

Standard TFTP transfer using 512-byte data blocks.

#### Octet Mode

Byte-oriented transfer mode for validating packet sequencing and ACK handling at the smallest granularity.

#### Netascii Mode

Supports newline translation:

LF → CRLF

to emulate traditional TFTP text transfer behaviour.

---

## Software Architecture

```text
                    +------------------+
                    |      Client      |
                    +------------------+
                             |
                             | RRQ / WRQ
                             v
                    +------------------+
                    | Server : 6969    |
                    +------------------+
                             |
                    Fork + Allocate Port
                             |
          +------------------+------------------+
          |                                     |
          v                                     v
    Port 20000                           Port 20001
  Child Process A                     Child Process B
          |                                     |
     File Transfer                        File Transfer
```

---

## Repository Structure

```text
TFTP-Client-Server/
│
├── client/
│   ├── tftp_client.c
│   └── tftp_client.h
│
├── server/
│   └── tftp_server.c
│
├── common/
│   ├── tftp.c
│   └── tftp.h
│
├── test_files/
│
├── README.md
├── LICENSE
└── .gitignore
```

---

## Build Instructions

### Build Server

```bash
gcc tftp_server.c tftp.c -o server
```

### Build Client

```bash
gcc tftp_client.c tftp.c -o client
```

---

## Execution

Start server:

```bash
./server
```

Start client:

```bash
./client
```

---

## Supported Commands

```text
connect
put
get
mode
exit
```

---

## Validation Performed

✔ File Upload (WRQ)

✔ File Download (RRQ)

✔ Normal Transfer Mode

✔ Octet Transfer Mode

✔ Netascii Transfer Mode

✔ Concurrent Client Sessions

✔ Dynamic Port Allocation

✔ ACK Validation

✔ Timeout Handling

✔ Retransmission Logic

✔ Error Packet Handling

---

## Concepts Demonstrated

* Linux System Programming
* UDP Socket Programming
* Application Layer Protocol Design
* Process Management (fork)
* Concurrent Server Architecture
* Reliable Data Transfer Mechanisms
* Network Protocol Implementation
* RFC-Based Development
* File I/O and Buffer Management

---

## Author

Fahed Shaikh

Embedded Systems Engineer

C | Linux Internals | Data Structures | Embedded C | CAN Protocol | Firmware Development
