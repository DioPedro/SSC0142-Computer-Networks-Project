# SSC0142-Computer-Networks-Project

## This project was made by:
- Diógenes Silva Pedro - 11883476
- Fernando Henrique Paes Generich - 11795342 
- Milena Corrêa da Silva - 11795401

The code has been tested and confirmed to work with Ubuntu 20.04, in a Linux native system or in a Windows Subsystem for Linux (WSL).

To compile the code we used gcc version 9.3.0 with includes the g++ compiler. We also used the make version 4.2.1.

Use the commands to compile the code:
```
make all
```

This will update the binary files of the server and of the client.

As for running the code you can start a server with:
```
./server
```
or
```
make run_server
```
The port is binded by default to 1337

For the client connection:
```
./client
```
or
```
make run_client
```

When the server is started it will make a log of the clients that has connected and which clients received a message.

The server also have a `/quit` command that logout every connected client.

When the client is added the first needed action is to set a nickname for the account. The usage is `/nickname <nick>`, the nickname can only containt at max 50 characters and a min of 1 char.

After that the client will have to enter the address (IP), if the client is in the same machine of the server, then the address can be localhost (127.0.0.1).

If the client is in a machine that not runs the server, the two machines will need to be in the same network. It's nice to comment that servers running in WSL machines will not be acessible because WSL is basically a VM and then it has it's own IP address.

As a last command the client will need to decide to `/connect` or to `/quit` the program.

After logged in into the server the client will have some options of things that can be done. To show those options use `/help`
```
/j #<channel>
/l #<channel>
/q
/p

Admin Commands:
/m <nickname>
/um <nickname>
/k <nickname>
/w <nickname>
```

All commands of the client and server:
```
Server:
/q

Client:
/j #<channel>
/l #<channel>
/q
/p

Admin Commands:
/m <nickname>
/um <nickname>
/k <nickname>
/w <nickname>
```

Ctrl+C doesn't work for closing the program, if the client tries to quit by using it a message will be printed saying how to logout.
<<<<<<< HEAD

The video explaining the code and showing the application can be seen [here](https://youtu.be/OAVp6GGQQlU)
=======
>>>>>>> da6b557ee5063d02f337f4d1fb6e1206999b6843
