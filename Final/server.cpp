#include <iostream>
#include <pthread.h>
#include <signal.h>
#include "server.hpp"
#include "socket.hpp"

using namespace std;

struct sClientList {
    int socket;
    bool received;
    int attempts;
    ClientList *prev;
    ClientList *next;
    char ip[INET6_ADDRSTRLEN];
    char name[NICKNAME_SIZE];
    bool isAdmin;
    bool muted;
    char channels[MAX_CHANNELS][CHANNEL_NAME_SIZE];
    int numberOfChannels;
    ChannelList *activeChannel;
    ClientList *activeInstance;
    ClientList *mainNode;
};

typedef struct sSendInfo {
    ClientList *node;
    char *message;
    ChannelList *channelRoot;
} SendInfo;

typedef struct sThreadInfo {
    ClientList *clientRoot;
    ClientList *clientNode;
    ChannelList *channelRoot;
} ThreadInfo;

struct sChannelList {
    ChannelList *prev;
    ChannelList *next;
    char name[CHANNEL_NAME_SIZE];
    ClientList *clients;
};

ClientList *createClient(int sockFd, char *ip) {
    ClientList *newNode = (ClientList *) malloc(sizeof(ClientList));
    newNode->socket = sockFd;
    newNode->received = false;
    newNode->attempts = 0;
    newNode->prev = NULL;
    newNode->next = NULL;
    strcpy(newNode->ip, ip);
    strcpy(newNode->name, "\0");
    newNode->isAdmin = false;
    newNode->muted = false;
    newNode->activeChannel = NULL;
    newNode->mainNode = NULL;
    newNode->numberOfChannels = 0;

    for (int i = 0; i < MAX_CHANNELS; ++i){
        newNode->channels[i][0] = '\0';
    }

    return newNode;
}

/* Creates a new channel node */
ChannelList *createChannelNode(char *name, ClientList *root) {
    ChannelList *newNode = (ChannelList *) malloc(sizeof(ChannelList));

    newNode->prev = NULL;
    newNode->next = NULL;
    strcpy(newNode->name, name);
    newNode->clients = root;

    return newNode;
}

/* Disconnect a client from the active channel */
void disconnectClient(ClientList *client, ChannelList *channel) {
    bool canCloseChannel = false;

    cout << "Disconnecting " << client->name << endl;
    close(client->socket);

    // Removing a client from the double linked list an mainting the links
    if (client->next == NULL)
        client->prev->next = NULL;
    else {
        client->prev->next = client->next;
        client->next->prev = client->prev;
    }

    if (client->numberOfChannels > 0) {
        ChannelList *tmp = channel->next;
        for (int i = 0; i < MAX_CHANNELS; ++i){
            if (client->channels[i][0] != '\0') {
                while (tmp != NULL) {
                    // Finding the channel the client is leaving
                    if (!strcmp(client->channels[i], tmp->name)) {
                        ClientList *clientTmp = tmp->clients->next;
                        while (clientTmp != NULL) {
                            if (!strcmp(client->name, clientTmp->name))
                                clientTmp->prev->next = clientTmp->next;
                                if (clientTmp->next != NULL)
                                    clientTmp->next->prev = clientTmp->prev;
                                // if there's no more clients if the channel it can be deleted
                                else if (clientTmp->prev->prev == NULL)
                                    canCloseChannel = true;

                            free(clientTmp);
                            clientTmp = NULL;
                            break;
                        }

                        clientTmp = clientTmp->next;
                    }

                    // Deleting a channel because there's no more clients in it
                    client->channels[i][0] = '\0';
                    if (canCloseChannel)
                        deleteChannel(tmp);
                    break;
                }

                tmp = tmp->next;
            }
        }
    }

    free(client);
    client = NULL;
}

/* Closes a channel when closing the server, remove all clients from the channel */
void closeChannel(ChannelList *channel) {
    cout << "\nClosing channel: " << channel->name << endl;

    ClientList *tmpClient;
    ClientList *root = channel->clients;

    // Removing the nodes from the channe;
    while (root != NULL) {
        cout << "\nRemoving " << root->name << " of channel" << endl;
        tmpClient = root;
        root = root->next;
        free(tmpClient);
        tmpClient = NULL;
    }
}

/* Deletes a channel when there's no more clients in it */
void deleteChannel(ChannelList *channel) {
    cout << "Deleting " << channel->name << endl;

    // Mantaining the links of the double linked list
    if (channel->next == NULL)
        channel->prev->next = NULL;
    else {
        channel->prev->next = channel->next;
        channel->next->prev = channel->prev;
    }

    free(channel);
    channel = NULL;
}

/* Prints an error message and closes the connection in case of failure */
void errorMessage(const char *message) {
    perror(message);
    exit(1);
}

/* Ignores default crtlCHandler */
void ctrlCHandler(int sig) {
    cout << "To exit use /q" << endl;
}

/* Add a newChar to the end of a string */
void addEndStr(char *str, char newchar) {
    int size = strlen(str);
    if (newchar == str[size - 1])
        return;
    if ((isalnum(str[size - 1])) || ispunct(str[size - 1]))
        str[size] = newchar;
    else
        str[size - 1] = newchar;
}

/* Closes the server */
void *closeServer(void *server) {
    ClientList *client = ((ThreadInfo *)server)->clientRoot;
    ChannelList *channel = ((ThreadInfo *)server)->channelRoot;
    ClientList *tmpClient;
    ChannelList *tmpChannel;

    // Listens the server side application
    while (true) {
        char input[MESSAGE_SIZE];
        cin >> input;

        // Closes all channels that still have clients in it
        if (!strcmp(input, "/q")) {
            while (channel != NULL) {
                closeChannel(channel);
                tmpChannel = channel;
                channel = channel->next;
                free(tmpChannel);
                tmpChannel = NULL;
            }

            // Closing all clients sockets
            while (client != NULL) {
                cout << "\nClose socktfd: " << client->socket << endl;
                close(client->socket);
                tmpClient = client;
                client = client->next;
                free(tmpClient);
                tmpClient = NULL;
            }

            free(server);
            server = NULL;
            cout << "Closing server..." << endl;
            exit(0);
        } else {
            cout << "Unknown command" << endl;
            cout << "\tCommand List:" << endl << "Quit: /q" << endl;
        }
    }
}

/* Sends server message to a Client */
void serverMessage(ChannelList *channel, ClientList *client, char message[]) {
    // Defining the thread with the message, the client, and the channel it needs to go
    pthread_t sendThread;
    SendInfo *sendInfo = (SendInfo *) malloc(sizeof(SendInfo));
    sendInfo->node = client;
    sendInfo->channelRoot = channel;
    sendInfo->message = (char *) malloc(sizeof(char) * (MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5));
    strcpy(sendInfo->message, message);

    // If there's no problem with the creation of the thread the message is sent to the client
    if (pthread_create(&sendThread, NULL, sendMessage, (void *)sendInfo))
        errorMessage("Create thread error");
    pthread_detach(sendThread);
}

/* Tries to send a message to a client 5 times. If unsuccessful disconnects the client */
void *sendMessage(void *info) {
    SendInfo *sendInfo = (SendInfo *)info;
    int snd;

    do {
        snd = send(sendInfo->node->socket, sendInfo->message, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5, 0);
        usleep(WAIT_ACK);
    } while (snd >= 0 && !sendInfo->node->mainNode->received && sendInfo->node->mainNode->attempts < 5);

    if (sendInfo->node->mainNode->attempts == 5 || snd < 0) {
        disconnectClient(sendInfo->node, sendInfo->channelRoot);
    } else if(sendInfo->node->mainNode->received) {
        sendInfo->node->mainNode->received = false;
        sendInfo->node->mainNode->attempts = 0;
        free(sendInfo->message);
        free(sendInfo);
    }
}

/* Sends the message to all clients */
void sendAllClients(ChannelList *channel, ClientList *clients, ClientList *client, char message[]) {
    ClientList *tmp = clients->next;
    int snd;

    // Going through every client to make a thread for each message
    while (tmp != NULL) {
        if (client->socket != tmp->socket) {
            cout << "Send to: " << tmp->name << " >> " << message;

            // Uses thread to send the message from the client to all clients in the same channel
            pthread_t sendThread;
            SendInfo *sendInfo = (SendInfo *) malloc(sizeof(SendInfo));
            sendInfo->node = tmp;
            sendInfo->channelRoot = channel;
            sendInfo->message = (char *) malloc(sizeof(char) * (MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5));
            strcpy(sendInfo->message, message);

            if (pthread_create(&sendThread, NULL, sendMessage, (void *)sendInfo) != 0)
                errorMessage("Create thread error");
            pthread_detach(sendThread);
        }
        tmp = tmp->next;
    }
}

/* Creates new channel if necessary, switches client channel to the one select */
void joinChannel(char *channelName, ChannelList *channel, ClientList *client) {
    ChannelList *tmp = channel;
    bool createNewChannel = true;

    // If the channel alredy exists no need to create a new one
    while (tmp->next != NULL) {
        if (!strcmp(channelName, tmp->next->name)) {
            createNewChannel = false;
            tmp = tmp->next;
            break;
        }

        tmp = tmp->next;
    }

    // Creating new channel
    char message[MESSAGE_SIZE];
    if (createNewChannel) {
        // Reached the max number of channels
        if (client->mainNode->numberOfChannels == MAX_CHANNELS) {
            sprintf(message, "Could not create %s. Limit of channels reached\n", channelName);
            serverMessage(channel, client->mainNode, message);
            return;
        }

        // Creating a new node to a new channel
        ClientList *newClient = createClient(client->socket, client->ip);
        newClient->mainNode = client->mainNode;
        strcpy(newClient->name, client->mainNode->name);
        newClient->isAdmin = true;

        ClientList *rootNode = createClient(0, "0");
        strcpy(rootNode->name, "root");
        ChannelList *newChannel = createChannelNode(channelName, rootNode);
        tmp->next = newChannel;
        newChannel->prev = tmp;

        newChannel->clients->next = newClient;
        newClient->prev = newChannel->clients;

        // Switching client channel
        client->mainNode->activeChannel = newChannel;
        client->mainNode->activeInstance = newClient;

        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (client->channels[i][0] == '\0') {
                strcpy(client->mainNode->channels[i], channelName);
                break;
            }
        }
        client->mainNode->numberOfChannels++;

        sprintf(message, "/channel %s Created and switched to channel", channelName);
        serverMessage(channel, client->mainNode, message);
    } else { // The channel alredy exists, just switch the client active channel
        bool isInChannel = false;
        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (!strcmp(client->channels[i], tmp->name)) {
                isInChannel = true;
                client->mainNode->activeChannel = tmp;

                ClientList *instance = tmp->clients;
                while (instance != NULL) {
                    if (!strcmp(instance->name, client->name)) {
                        client->mainNode->activeInstance = instance;
                        break;
                    }

                    instance = instance->next;
                }

                sprintf(message, "/channel %s Switched to channel", channelName);
                serverMessage(channel, client->mainNode, message);

                break;
            }
        }

        if (!isInChannel) {
            if (client->mainNode->numberOfChannels == MAX_CHANNELS) {
                sprintf(message, "Could not join %s. Limit of channels reached.\n", channelName);
                serverMessage(channel, client->mainNode, message);
                return;
            }

            ClientList *newClient = createClient(client->socket, client->ip);
            newClient->mainNode = client->mainNode;
            strcpy(newClient->name, client->mainNode->name);

            newClient->next = tmp->clients->next->next;
            newClient->prev = tmp->clients->next;
            if (tmp->clients->next->next != NULL)
                tmp->clients->next->next->prev = newClient;
            tmp->clients->next->next = newClient;

            client->mainNode->activeChannel = tmp;
            client->mainNode->activeInstance = newClient;

            for (int i = 0; i < MAX_CHANNELS; ++i) {
                if (client->mainNode->channels[i][0] == '\0') {
                    strcpy(client->mainNode->channels[i], channelName);
                    break;
                }
            }

            client->mainNode->numberOfChannels++;

            sprintf(message, "/channel %s You joined the channel", channelName);
            serverMessage(channel, newClient->mainNode, message);

            sprintf(message, "%s - %s joined the channel.\n", channelName, newClient->name);
            sendAllClients(channel, tmp->clients, newClient->mainNode, message);
        }
    }
}

/* Tell the admin of a channel the IP of a specific client */
bool whoIs(ClientList *admin, char *username) {
    char buffer[MESSAGE_SIZE] = {};
    ClientList *tmp = admin->mainNode->activeChannel->clients->next;

    // Finding the client that we want
    while (tmp != NULL) {
        if (!strcmp(tmp->name, username)) {
            sprintf(buffer, "%s - User(%s): %s\n", admin->mainNode->activeChannel->name, username, tmp->ip);
            int snd = send(admin->socket, buffer, MESSAGE_SIZE, 0);
            if (snd < 0)
                return false;
            return true;
        }

        tmp = tmp->next;
    }

    // The client isn't in the channel
    sprintf(buffer, "User '%s' is not on this channel\n", username);
    int snd = send(admin->socket, buffer, MESSAGE_SIZE, 0);
    if (snd < 0)
        return false;
    return true;
}

/* Allows admin to mute/unmute a client in the channel */
void muteUser(ChannelList *channel, ClientList *admin, char *username, bool mute) {
    ClientList *tmp = admin->mainNode->activeChannel->clients->next;
    char message[MESSAGE_SIZE] = {};

    // Finding the client
    while (tmp != NULL) {
        if (!strcmp(tmp->name, username)) {
            tmp->muted = mute;
            if (mute) {
                sprintf(message, "%s - You were muted by %s.\n", admin->mainNode->activeChannel->name, admin->name);
                serverMessage(channel, tmp->mainNode, message);

                sprintf(message, "%s - %s was muted.\n", admin->mainNode->activeChannel->name, tmp->name);
                serverMessage(channel, admin->mainNode, message);
            } else {
                sprintf(message, "%s - You were unmuted by %s.\n", admin->mainNode->activeChannel->name, admin->name);
                serverMessage(channel, tmp->mainNode, message);

                sprintf(message, "%s - %s was unmuted.\n", admin->mainNode->activeChannel->name, tmp->name);
                serverMessage(channel, admin->mainNode, message);
            }

            return;
        }
        tmp = tmp->next;
    }

    // If the client isn't in the channel
    sprintf(message, "User '%s' is not on this channel\n", username);
    send(admin->socket, message, MESSAGE_SIZE, 0);
}

/* Allows admin to kick client from the channel */
void kickUser(ChannelList *channel, ClientList *admin, char *username) {
    bool canCloseChannel = false;
    ClientList *tmp = admin->mainNode->activeChannel->clients->next->next;
    char message[MESSAGE_SIZE];

    // Finding the client
    while (tmp != NULL) {
        if (!strcmp(tmp->name, username)) {
            tmp->prev->next = tmp->next;
            if (tmp->next != NULL)
                tmp->next->prev = tmp->prev;
            else if (tmp->prev->prev == NULL)
                canCloseChannel = true;

            for (int i = 0; i < MAX_CHANNELS; ++i) {
                if (!strcmp(tmp->mainNode->channels[i], admin->mainNode->activeChannel->name)) {
                    tmp->mainNode->channels[i][0] = '\0';
                    break;
                }
            }

            // Message to the kicked client
            sprintf(message, "/channel #none You were kicked out of the channel %s by %s. Switched to", admin->mainNode->activeChannel->name, admin->name);
            serverMessage(channel, tmp->mainNode, message);

            // Message to all clients in the server
            sprintf(message, "%s - %s were kicked out of the channel.\n", admin->mainNode->activeChannel->name, tmp->name);
            sendAllClients(channel, admin->mainNode->activeChannel->clients, tmp->mainNode, message);

            tmp->mainNode->activeChannel = channel;
            tmp->mainNode->activeInstance = tmp->mainNode;

            tmp->mainNode->numberOfChannels--;

            free(tmp);
            tmp = NULL;
            if (canCloseChannel)
                deleteChannel(admin->mainNode->activeChannel);
            return;
        }

        tmp = tmp->next;
    }

    sprintf(message, "User '%s' is not on this channel\n", username);
    send(admin->socket, message, MESSAGE_SIZE, 0);
}

/* Client can leave the specified server, but still continues connected to the server */
void leaveChannel(ChannelList *channel, ClientList *client, char *channelName) {
    char message[MESSAGE_SIZE];
    bool canCloseChannel = false;

    ChannelList *tmpChannel = channel->next;

    while (tmpChannel != NULL) {
        if (!strcmp(tmpChannel->name, channelName))
            break;
        
        tmpChannel = tmpChannel->next;
    }

    if (tmpChannel != NULL) {
        ClientList *tmp = tmpChannel->clients->next;
        while (tmp != NULL) {
            if (!strcmp(client->name, tmp->name)) {
                tmp->prev->next = tmp->next;
                if (tmp->next != NULL)
                    tmp->next->prev = tmp->prev;
                else if (tmp->prev->prev == NULL)
                    canCloseChannel = true;
            }

            for (int i = 0; i < MAX_CHANNELS; ++i) {
                if (!strcmp(tmp->mainNode->channels[i], tmpChannel->name)) {
                    // If the channel will still existing and the admin leaves it, change the admin
                    if (tmpChannel->clients->next != NULL)
                        tmpChannel->clients->next->isAdmin = true;
                    tmp->mainNode->channels[i][0] = '\0';
                    break;
                }
            }

            // Client who leaved the channel message
            sprintf(message, "You left the channel %s.\n", tmpChannel->name);
            serverMessage(channel, tmp->mainNode, message);

            tmp->mainNode->numberOfChannels--;

            // Clients who stayed in the channel message
            sprintf(message, "%s - %s left the channel.\n", tmpChannel->name, tmp->name);
            sendAllClients(channel, tmpChannel->clients, tmp->mainNode, message);

            if (!strcmp(client->mainNode->activeChannel->name, channelName)) {
                tmp->mainNode->activeChannel = channel;
                tmp->mainNode->activeInstance = tmp->mainNode;

                sprintf(message, "/channel #none You left the channel %s. Switched to", tmpChannel->name);
                serverMessage(channel, tmp->mainNode, message);
            }

            free(tmp);
            tmp = NULL;
            if (canCloseChannel)
                deleteChannel(tmpChannel);
            return;
        }

        tmp = tmp->next;
    } else {
        sprintf(message, "This channel does not exist.\n");
        serverMessage(channel, client->mainNode, message);
    }
}

/* Thread function for receiving messages from the client */
void *clientHandler(void *info) {
    int leaveFlag = 0;
    char recvBuffer[MESSAGE_SIZE] = {};
    char sendBuffer[MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5] = {};
    ThreadInfo *tInfo = (ThreadInfo *)info;
    char message[MESSAGE_SIZE] = {};
    char command[MESSAGE_SIZE] = {};
    char argument[MESSAGE_SIZE] = {};

    cout << tInfo->clientNode->name << " (" << tInfo->clientNode->ip << ")"
         << " (" << tInfo->clientNode->socket << ")"
         << " joined the server.\n";
    sprintf(sendBuffer, "Server: %s joined the server.     \n", tInfo->clientNode->name);
    sendAllClients(tInfo->channelRoot, tInfo->clientRoot, tInfo->clientNode, sendBuffer);

    while (true) {
        if (leaveFlag)
            break;

        bzero(recvBuffer, MESSAGE_SIZE);
        bzero(sendBuffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5);
        bzero(command, MESSAGE_SIZE);
        bzero(argument, MESSAGE_SIZE);

        int rcv = recv(tInfo->clientNode->socket, recvBuffer, MESSAGE_SIZE, 0);

        if (rcv <= 0) {
            leaveFlag = 1;
            continue;
        }

        // Checks for the command the client is requesting for the server
        if (recvBuffer[0] == '/') {
            sscanf(recvBuffer, "%s %s", command, argument);
            addEndStr(command, '\0');
            addEndStr(argument, '\0');

            if (!strcmp(command, "/ack")) {
                tInfo->clientNode->received = true;
                tInfo->clientNode->attempts = 0;
                cout << tInfo->clientNode->name << " received the message" << endl;
            } else if (!strcmp(command, "/q")) {
                cout << tInfo->clientNode->name << " (" << tInfo->clientNode->ip << ")"
                     << " (" << tInfo->clientNode->socket << ")"
                     << " left the server.\n";
                sprintf(sendBuffer, "Server: %s left the server.     \n", tInfo->clientNode->name);
                sendAllClients(tInfo->channelRoot, tInfo->clientRoot, tInfo->clientNode, sendBuffer);
                leaveFlag = 1;
            } else if (!strcmp(command, "/p")) {
                sprintf(sendBuffer, "Server: pong\n");
                serverMessage(tInfo->channelRoot, tInfo->clientNode, sendBuffer);
            } else if (!strcmp(command, "/j")) {
                if (argument[0] == '&' || argument[0] == '#') {
                    joinChannel(argument, tInfo->channelRoot, tInfo->clientNode);
                } else {
                    sprintf(message, "Incorrect name form. Channel name needs to start with '#' or '&'.\n.");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode, message);
                }
            } else if (!strcmp(command, "/l")) {
                if (argument[0] == '&' || argument[0] == '#') {
                    leaveChannel(tInfo->channelRoot, tInfo->clientNode, argument);
                } else {
                    sprintf(message, "Incorrect name form. Channel name needs to start with '#' or '&'.\n.");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode, message);
                }
            } else if (!strcmp(command, "/w")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    if (!whoIs(tInfo->clientNode->activeInstance, argument)) {
                        leaveFlag = 1;
                    }
                } else {
                    sprintf(message, "Invalid command. You are not the admin of this channel.\n");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/k")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    kickUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument);
                } else {
                    sprintf(message, "Invalid command. You are not the admin of this channel.\n");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/m")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    muteUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument, true);
                } else {
                    sprintf(message, "Invalid command. You are not the admin of this channel.\n");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/um")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    muteUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument, false);
                } else {
                    sprintf(message, "Invalid command. You are not the admin of this channel.\n");
                    serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/h")) {
                sprintf(message,
                        "    User commands:\n/q = quit program\n/p = check connection\n/j channelName = join a channel\n/l channelName = leave a channel\n\n    Admin commands:\n/w user = show user's ip\n/m user = diable user's messages on the channel\n/um user = enables user's messages on the channel\n/k user = kicks user from channel\n");
                serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
            } else {
                sprintf(message, "Unknown command. Use /h to see the list of commands.\n");
                serverMessage(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
            }
        } else if (!tInfo->clientNode->activeInstance->muted) {
            sprintf(sendBuffer, "%s - %s: %s", tInfo->clientNode->activeChannel->name, tInfo->clientNode->name, recvBuffer);
            sendAllClients(tInfo->channelRoot, tInfo->clientNode->activeChannel->clients, tInfo->clientNode->activeInstance, sendBuffer);
        }
    }

    // If leave flag turns true client will be disconnected
    disconnectClient(tInfo->clientNode, tInfo->channelRoot);
    free(tInfo);
}

int main() {
    int serverFd = 0, clientFd = 0;
    int opt = 1;
    char nickname[NICKNAME_SIZE] = {};
    ThreadInfo *info = NULL;

    signal(SIGINT, ctrlCHandler);

    // Creates file descriptor for the socket
    if ((serverFd = socket(AF_INET, SOCK_STREAM, PROTOCOL)) < 0)
        errorMessage("Socket failed.");
    
    // Forcing the socket to be attached to port 8080
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        errorMessage("setsockopt");

    struct sockaddr_in serverAddr, clientAddr;
    socklen_t serverAddrSize = sizeof(serverAddr);
    socklen_t clientAddrSize = sizeof(clientAddr);
    bzero((char *)&serverAddr, serverAddrSize);
    bzero((char *)&clientAddr, clientAddrSize);

    // Seting up the structure of the serverAdd to be used in bind call
    // Server byte order
    serverAddr.sin_family = AF_INET;
    // Fill automatically with the current host's IP address
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    // Converting short int value from host to network byte order
    serverAddr.sin_port = htons(PORT);

    // Binds the socket to the current IP address on port
    if (bind(serverFd, (struct sockaddr *)&serverAddr, serverAddrSize) < 0)
        errorMessage("bind failed");

    // Telling the socket to listen incoming connections. Maximum size for the backlog queue is 5
    if (listen(serverFd, MAX_CONNECTIONS) < 0)
        errorMessage("listen");

    // Printing the server IP
    getsockname(serverFd, (struct sockaddr *)&serverAddr, (socklen_t *)&serverAddrSize);
    cout << "Start Server on: " << inet_ntoa(serverAddr.sin_addr) << ": " << ntohs(serverAddr.sin_port) << endl;

    // Creating the initial double linked list for the clients, the root is the server
    ClientList *clientRoot = createClient(serverFd, inet_ntoa(serverAddr.sin_addr));

    ClientList *channelRootAdmin = createClient(0, "0");
    strcpy(channelRootAdmin->name, "root");
    ChannelList *channelRoot = createChannelNode("#root", channelRootAdmin);

    info = (ThreadInfo *) malloc(sizeof(ThreadInfo));
    info->clientRoot = clientRoot;
    info->clientNode = NULL;
    info->channelRoot = channelRoot;

    // Thread that will catch the server input, used to catch the quit command '/q'
    pthread_t inputThreadId;
    if (pthread_create(&inputThreadId, NULL, closeServer, (void *)info) != 0)
        errorMessage("input thread ERROR");
    
    // Server's running and accepting new clients
    while (true) {
        if ((clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientAddrSize)) < 0) {
            cout << "Error accepting client" << endl;
            continue;
        }

        // Printing the client IP
        cout << "Client " << inet_ntoa(clientAddr.sin_addr) << " : " << ntohs(clientAddr.sin_port) << " joined." << endl;

        // Create new client and append it to the double linked list
        ClientList *node = createClient(clientFd, inet_ntoa(clientAddr.sin_addr));
        node->mainNode = node;
        node->activeInstance = node;
        node->muted = true;

        // Naming the client
        recv(node->socket, nickname, NICKNAME_SIZE, 0);
        strcpy(node->name, nickname);

        ClientList *last = clientRoot;
        while (last->next != NULL) {
            last = last->next;
        }

        node->prev = last;
        last->next = node;

        bzero(nickname, NICKNAME_SIZE);

        info = (ThreadInfo *) malloc(sizeof(ThreadInfo));
        info->clientRoot = clientRoot;
        info->clientNode = node;
        info->channelRoot = channelRoot;

        // Each client will have a new thread
        pthread_t id;
        if (pthread_create(&id, NULL, clientHandler, (void *)info)) {
            cout << "Create pthread error" << endl;

            // Removing the client if necessary
            disconnectClient(node, channelRoot);
            pthread_detach(id);
        }
    }

    return 0;
}