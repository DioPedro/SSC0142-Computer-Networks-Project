#ifndef SOCKET_H
#define SOCKET_H

#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 1337
#define PROTOCOL 0
#define MESSAGE_SIZE 2049
#define NICKNAME_SIZE 51
#define CHANNEL_NAME_SIZE 201

void errorMessage(const char *message);
void ctrlCHandler(int signal);
void strTrim(char *str, char newChar);

#endif