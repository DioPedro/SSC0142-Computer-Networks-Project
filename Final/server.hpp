#ifndef SERVER_H
#define SERVER_H

#define MAX_CONNECTIONS 5
#define MAX_CHANNELS 10
#define WAIT_ACK 400000

typedef struct sChannelList ChannelList;
typedef struct sClientList ClientList;

ClientList *createClient(int sockFd, char *ip);
ChannelList *createChannelNode(char *name, ClientList *root);
void disconnectClient(ClientList *client, ChannelList *channel);
void deleteChannel(ChannelList *channel);
void *closeServer(void *server);
void serverMessage(ChannelList *channel, ClientList *client, char message[]);
void *sendMessage(void *info);
void sendAllClients(ChannelList *channel, ClientList *clients, ClientList *client, char message[]);
void joinChannel(char *channelName, ChannelList *channel, ClientList *client);
void muteUser(ChannelList *channel, ClientList *admin, char *username, bool mute);
void kickUser(ChannelList *channel, ClientList *admin, char *username);
void leaveChannel(ChannelList *channel, ClientList *client, char *channelName);
void *clientHandler(void *info);
bool whoIs(ClientList *admin, char *username);

#endif