# SSC0142-Computer-Networks-Project

## This project was made by:
- Diógenes Silva Pedro - 11883476
- Fernando Henrique Paes Generich - 11795342 
- Milena Corrêa da Silva - 11795401

The code has been tested and confirmed to work with Ubuntu 20.04, in a Linux native system or in a Windows Subsystem for Linux (WSL).

Video showing the code and the program in operation: https://youtu.be/OAVp6GGQQlU

To compile the code we used gcc version 9.3.0 with includes the g++ compiler. We also used the make version 4.2.1.

Use the commands to compile the code:
```
make server
make client
```

This will update the binary files of the server and of the client.

As for running the code you can start a server with:
```
./server <port>
```
The port means which port you choose

For the client connection:`
```
./client <server_ip> <port>
```
The client can connect to the server in the same machine then server_ip = localhost, but it can also be used through different machines connected to a same network by having the IP of the machine that's hosting the server. 

The port has to be the same from the server, else the connection won't work.
