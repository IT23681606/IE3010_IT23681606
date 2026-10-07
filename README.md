# IE3010 NetMessenger

## Student Information

Registration Number: IT23681606

## Personalisation

Last four digits: 1606
Server/Client TCP Port: 7606
Node ID (NID): 6816

Server Source File: server_1606.c
Client Source File: client_1606.c
Makefile: Makefile_1606
Log File: netmsg_IT23681606.log
Storage Path: ./storage/IT23681606/
Submission Archive: IE3010_IT23681606.zip

## Project Description

NetMessenger is a multi-client chat and file-sharing platform implemented
using C socket programming over TCP/IP.

The system consists of a server and multiple clients. The server manages
client connections, user registration, messaging, chat rooms, file sharing,
error handling and server-side logging.

## Features

- Multiple simultaneous TCP clients
- Unique username registration
- User join and leave notifications
- Broadcast messaging
- Private messaging
- Chat rooms
- File sharing
- Graceful client disconnection
- Error handling
- Server-side event logging

## Build Instructions

Compile the server:

gcc -Wall -Wextra server_1606.c -o server_1606 -pthread

Compile the client:

gcc -Wall -Wextra client_1606.c -o client_1606 -pthread

## Run the Server

./server_1606

The server listens on TCP port 7606.

## Run the Client

./client_1606

The client connects to the NetMessenger server using TCP port 7606.

## Storage

Received files are stored under:

./storage/IT23681606/

## Development Environment

Operating System: CentOS Linux
Programming Language: C
Network Protocol: TCP/IP
Compiler: GCC
Version Control: Git
