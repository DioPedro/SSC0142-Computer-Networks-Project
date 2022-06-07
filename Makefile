CC = gcc
SERVER = server.cpp
SBINARY = ./server
CLIENT = client.cpp 
CBINARY = ./client

server:
	@$(CC) $(SERVER) -o $(SBINARY)

client:
	@$(CC) $(CLIENT) -o $(CBINARY)