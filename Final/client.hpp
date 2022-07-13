#ifndef CLIENT_H
#define CLIENT_H

#define LOCALHOST "127.0.0.1"

void quit(int sock);
void printNickname();
void *receiveMessageHandler(void *sock);
void *sendMessageHandler(void *sock);

#endif