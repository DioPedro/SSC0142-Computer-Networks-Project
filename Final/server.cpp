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

ChannelList *createChannelNode(char *name, ClientList *root) {
    ChannelList *newNode = (ChannelList *) malloc(sizeof(ChannelList));

    newNode->prev = NULL;
    newNode->next = NULL;
    strcpy(newNode->name, name);
    newNode->clients = root;

    return newNode;
}

void printNode(ClientList *node) {
    cout << "\n\tPrinting node: " << endl;
    if (node->prev == NULL)
        cout << "Node prev = NULL" << endl;
    else
        cout << "Node prev = " << node->prev->name << endl;
    
    cout << "Node = " << node->name << endl;

    if (node->next == NULL)
        cout << "Node next = NULL" << endl;
    else
        cout << "Node next = " << node->next->name << endl;
}

void disconnectClient(ClientList *client, ChannelList *channel) {
    bool canCloseChannel = false;

    cout << "Disconnecting " << client->name << endl;
    close(client->socket);

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
                    if (!strcmp(client->channels[i], tmp->name)) {
                        ClientList *clientTmp = tmp->clients->next;
                        while (clientTmp != NULL) {
                            if (!strcmp(client->name, clientTmp->name))
                                clientTmp->prev->next = clientTmp->next;
                                if (clientTmp->next != NULL)
                                    clientTmp->next->prev = clientTmp->prev;
                                else if (clientTmp->prev->prev == NULL)
                                    canCloseChannel = true;

                            free(clientTmp);
                            clientTmp = NULL;
                            break;
                        }

                        clientTmp = clientTmp->next;
                    }
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

void closeChannel(ChannelList *channel) {
    cout << "\nClosing channel: " << channel->name << endl;

    ClientList *tmpClient;
    ClientList *root = channel->clients;

    while (root != NULL) {
        cout << "\nRemoving " << root->name << " of channel" << endl;
        tmpClient = root;
        root = root->next;
        free(tmpClient);
        tmpClient = NULL;
    }
}

void deleteChannel(ChannelList *channel) {
    cout << "Deleting " << channel->name << endl;

    if (channel->next == NULL)
        channel->prev->next = NULL;
    else {
        channel->prev->next = channel->next;
        channel->next->prev = channel->prev;
    }

    free(channel);
    channel = NULL;
}

void errorMessage(const char *message) {
    perror(message);
    exit(1);
}

void ctrlCHandler(int sig) {
    cout << "To exit use /quit" << endl;
}

void strTrim(char *str, char newchar) {
    int size = strlen(str);
    if (newchar == str[size - 1])
        return;
    if ((isalnum(str[size - 1])) || ispunct(str[size - 1]))
        str[size] = newchar;
    else
        str[size - 1] = newchar;
}

void *closeServer(void *server) {
    ClientList *client = ((ThreadInfo *)server)->clientRoot;
    ChannelList *channel = ((ThreadInfo *)server)->channelRoot;
    ClientList *tmpClient;
    ChannelList *tmpChannel;

    while (true) {
        char input[MESSAGE_SIZE];
        cin >> input;

        if (!strcmp(input, "/quit")) {
            while (channel != NULL) {
                closeChannel(channel);
                tmpChannel = channel;
                channel = channel->next;
                free(tmpChannel);
                tmpChannel = NULL;
            }

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
            cout << "\tCommand List:" << endl << "Quit: /quit" << endl;
        }
    }
}

void send(ChannelList *channel, ClientList *client, char message[]) {
    pthread_t sendThread;
    SendInfo *sendInfo = (SendInfo *) malloc(sizeof(SendInfo));
    sendInfo->node = client;
    sendInfo->channelRoot = channel;
    sendInfo->message = (char *) malloc(sizeof(char) * (MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5));
    strcpy(sendInfo->message, message);

    if (pthread_create(&sendThread, NULL, sendMessage, (void *)sendInfo))
        errorMessage("Create thread error");
    pthread_detach(sendThread);
}

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

void sendAllClients(ChannelList *channel, ClientList *clients, ClientList *client, char message[]) {
    ClientList *tmp = clients->next;
    int snd;

    while (tmp != NULL) {
        if (client->socket != tmp->socket) {
            cout << "Send to: " << tmp->name << " >> " << message;

            pthread_t sendThread;
            SendInfo *sendInfo = (SendInfo *) malloc(sizeof(SendInfo));
            sendInfo->node = tmp;
            sendInfo->channelRoot = channel;
            sendInfo->message = (char *) malloc(sizeof(char) * (MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5));
            strcpy(sendInfo->message, message);

            if (pthread_create(&sendThread, NULL, sendMessage, (void *)sendInfo))
                errorMessage("Createe thread error");
            pthread_detach(sendThread);
        }
        tmp = tmp->next;
    }
}

void joinChannel(char *channelName, ChannelList *channel, ClientList *client) {
    ChannelList *tmp = channel;
    bool createNewChannel = true;

    while (tmp->next != NULL) {
        if (!strcmp(channelName, tmp->next->name)) {
            createNewChannel = false;
            tmp = tmp->next;
            break;
        }

        tmp = tmp->next;
    }

    char message[MESSAGE_SIZE];
    if (createNewChannel) {
        if (client->mainNode->numberOfChannels == MAX_CHANNELS) {
            sprintf(message, "Could not create %s. Limit of channels reached\n", channel);
            send(channel, client->mainNode, message);
            return;
        }

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

        client->mainNode->activeChannel = newChannel;
        client->mainNode->activeInstance = newClient;

        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (client->channels[i][0] == '\0') {
                strcpy(client->mainNode->channels[i], channelName);
                break;
            }
        }
        client->mainNode->numberOfChannels++;

        sprintf(message, "/channel %s Created and switched to channel", channel);
        send(channel, client->mainNode, message);
    } else {
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
                send(channel, client->mainNode, message);

                break;
            }
        }

        if (!isInChannel) {
            if (client->mainNode->numberOfChannels == MAX_CHANNELS) {
                sprintf(message, "Could not join %s. Limit of channels reached.\n", channelName);
                send(channel, client->mainNode, message);
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
            send(channel, newClient->mainNode, message);

            sprintf(message, "%s - %s joined the channel.\n", channelName, newClient->name);
            sendAllClients(channel, tmp->clients, newClient->mainNode, message);
        }
    }
}

bool whoIs(ClientList *admin, char *username) {
    char buffer[MESSAGE_SIZE] = {};
    ClientList *tmp = admin->mainNode->activeChannel->clients->next;

    while (tmp != NULL) {
        if (!strcmp(tmp->name, username)) {
            sprintf(buffer, "%s - User(%s): %s\n", admin->mainNode->activeChannel->name, username, tmp->ip);
            if (send(admin->socket, buffer, MESSAGE_SIZE, 0) < 0)
                return false;
            return true;
        }

        tmp = tmp->next;
    }

    sprintf(buffer, "User '%s' is not on this channel\n", username);
    if (send(admin->socket, buffer, MESSAGE_SIZE, 0) < 0)
        return false;
    return true;
}

void muteUser(ChannelList *channel, ClientList *admin, char *username, bool mute) {
    ClientList *tmp = admin->mainNode->activeChannel->clients->next;
    char message[MESSAGE_SIZE] = {};

    while (tmp != NULL) {
        if (!strcmp(tmp->name, username)) {
            tmp->muted = mute;
            if (mute) {
                sprintf(message, "%s - You were muted by %s.\n", admin->mainNode->activeChannel->name, admin->name);
                send(channel, tmp->mainNode, message);

                sprintf(message, "%s - %s was muted.\n", admin->mainNode->activeChannel->name, tmp->name);
                send(channel, admin->mainNode, message);
            } else {
                sprintf(message, "%s - You were unmuted by %s.\n", admin->mainNode->activeChannel->name, admin->name);
                send(channel, tmp->mainNode, message);

                sprintf(message, "%s - %s was unmuted.\n", admin->mainNode->activeChannel->name, tmp->name);
                send(channel, admin->mainNode, message);
            }

            return;
        }
        tmp = tmp->next;
    }

    sprintf(message, "User '%s' is not on this channel\n", username);
    send(admin->socket, message, MESSAGE_SIZE, 0);
}

void kickUser(ChannelList *channel, ClientList *admin, char *username) {
    bool canCloseChannel = false;
    ClientList *tmp = admin->mainNode->activeChannel->clients->next->next;
    char message[MESSAGE_SIZE];

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

            sprintf(message, "/channel #none You were kicked out of the channel %s by %s. Switched to", admin->mainNode->activeChannel->name, admin->name);
            send(channel, tmp->mainNode, message);

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
                    tmp->mainNode->channels[i][0] = '\0';
                    break;
                }
            }

            sprintf(message, "You left the channel %s.\n", tmpChannel->name, tmp->name);
            send(channel, tmp->mainNode, message);

            tmp->mainNode->numberOfChannels--;

            sprintf(message, "%s - %s left the channel.\n", tmpChannel->name, tmp->name);
            sendAllClients(channel, tmpChannel->clients, tmp->mainNode, message);

            if (!strcmp(client->mainNode->activeChannel->name, channelName)) {
                tmp->mainNode->activeChannel = channel;
                tmp->mainNode->activeInstance = tmp->mainNode;

                sprintf(message, "/channel #none You left the channel %s. Switched to", tmpChannel->name);
                send(channel, tmp->mainNode, message);
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
        send(channel, client->mainNode, message);
    }
}

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

        if (recvBuffer[0] == '/') {
            sscanf(recvBuffer, "%s %s", command, argument);

            strTrim(command, '\0');
            strTrim(argument, '\0');

            if (!strcmp(command, "/ack")) {
                tInfo->clientNode->received = true;
                tInfo->clientNode->attempts = 0;
                cout << tInfo->clientNode->name << " received the message" << endl;
            } else if (!strcmp(command, "/quit")) {
                cout << tInfo->clientNode->name << " (" << tInfo->clientNode->ip << ")"
                     << " (" << tInfo->clientNode->socket << ")"
                     << " left the server.\n";
                sprintf(sendBuffer, "Server: %s left the server.     \n", tInfo->clientNode->name);
                sendAllClients(tInfo->channelRoot, tInfo->clientRoot, tInfo->clientNode, sendBuffer);
                leaveFlag = 1;
            } else if (!strcmp(command, "/ping")) {
                sprintf(sendBuffer, "Server: pong\n");
                send(tInfo->channelRoot, tInfo->clientNode, sendBuffer);
            } else if (!strcmp(command, "/join")) {
                if (argument[0] == '&' || argument[0] == '#') {
                    joinChannel(argument, tInfo->channelRoot, tInfo->clientNode);
                } else {
                    sprintf(message, "Incorrect name form. Channel name needs to start with '#' or '&'.\n.");
                    send(tInfo->channelRoot, tInfo->clientNode, message);
                }
            } else if (!strcmp(command, "/leave")) {
                if (argument[0] == '&' || argument[0] == '#') {
                    leaveChannel(tInfo->channelRoot, tInfo->clientNode, argument);
                } else {
                    sprintf(message, "Incorrect name form. Channel name needs to start with '#' or '&'.\n.");
                    send(tInfo->channelRoot, tInfo->clientNode, message);
                }
            } else if (!strcmp(command, "/whois")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    if (!whoIs(tInfo->clientNode->activeInstance, argument)) {
                        leaveFlag = 1;
                    }
                } else {
                    sprintf(message, "Invalid command. You are not the admin of this channel.\n");
                    send(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/kick")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    kickUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument);
                } else {
                    sprintf(message, "Invalid command. You are not the admin of this channel.\n");
                    send(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/mute")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    muteUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument, true);
                } else {
                    sprintf(message, "Invalid command. You are not the admin of this channel.\n");
                    send(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/unmute")) {
                if (tInfo->clientNode->activeInstance->isAdmin) {
                    muteUser(tInfo->channelRoot, tInfo->clientNode->activeInstance, argument, false);
                } else {
                    sprintf(message, "Invalid command. You are not the admin of this channel.\n");
                    send(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
                }
            } else if (!strcmp(command, "/help")) {
                sprintf(message,
                        "    User commands:\n/quit = quit program\n/ping = check connection\n/join channelName = join a channel\n/leave channelName = leave a channel\n\n    Admin commands:\n/whois username = show user's ip\n/mute username = diable user's messages on the channel\n/unmute username = enables user's messages on the channel\n/kick username = kicks user from channel\n");
                send(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
            } else {
                sprintf(message, "Unknown command. Use /help to see the list of commands.\n");
                send(tInfo->channelRoot, tInfo->clientNode->activeInstance, message);
            }
        } else if (!tInfo->clientNode->activeInstance->muted) {
            sprintf(sendBuffer, "%s - %s: %s", tInfo->clientNode->activeChannel->name, tInfo->clientNode->name, recvBuffer);
            sendAllClients(tInfo->channelRoot, tInfo->clientNode->activeChannel->clients, tInfo->clientNode->activeInstance, sendBuffer);
        }
    }

    disconnectClient(tInfo->clientNode, tInfo->channelRoot);
    free(tInfo);
}

int main() {
    int serverFd = 0, clientFd = 0;
    int opt = 1;
    char nickname[NICKNAME_SIZE] = {};
    ThreadInfo *info = NULL;

    signal(SIGINT, ctrlCHandler);

    if ((serverFd = socket(AF_INET, SOCK_STREAM, PROTOCOL)) < 0)
        errorMessage("Socket failed.");
    
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        errorMessage("setsockopt");

    struct sockaddr_in serverAddr, clientAddr;
    socklen_t serverAddrSize = sizeof(serverAddr);
    socklen_t clientAddrSize = sizeof(clientAddr);
    bzero((char *)&serverAddr, serverAddrSize);
    bzero((char *)&clientAddr, clientAddrSize);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverFd, (struct sockaddr *)&serverAddr, serverAddrSize) < 0)
        errorMessage("bind failed");

    if (listen(serverFd, MAX_CONNECTIONS) < 0)
        errorMessage("listen");

    getsockname(serverFd, (struct sockaddr *)&serverAddr, (socklen_t *)&serverAddrSize);
    cout << "Start Server on: " << inet_ntoa(serverAddr.sin_addr) << ": " << ntohs(serverAddr.sin_port) << endl;

    ClientList *clientRoot = createClient(serverFd, inet_ntoa(serverAddr.sin_addr));

    ClientList *channelRootAdmin = createClient(0, "0");
    strcpy(channelRootAdmin->name, "root");
    ChannelList *channelRoot = createChannelNode("#root", channelRootAdmin);

    info = (ThreadInfo *) malloc(sizeof(ThreadInfo));
    info->clientRoot = clientRoot;
    info->clientNode = NULL;
    info->channelRoot = channelRoot;

    pthread_t inputThreadId;
    if (pthread_create(&inputThreadId, NULL, closeServer, (void *)info) != 0)
        errorMessage("input thread ERROR");

    while (true) {
        if ((clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientAddrSize)) < 0) {
            cout << "Error accepting client" << endl;
            continue;
        }

        cout << "Client " << inet_ntoa(clientAddr.sin_addr) << " : " << ntohs(clientAddr.sin_port) << " joined." << endl;

        ClientList *node = createClient(clientFd, inet_ntoa(clientAddr.sin_addr));
        node->mainNode = node;
        node->activeInstance = node;
        node->muted = true;

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

        pthread_t id;
        if (pthread_create(&id, NULL, clientHandler, (void *)info)) {
            cout << "Create pthread error" << endl;

            disconnectClient(node, channelRoot);
            pthread_detach(id);
        }
    }

    return 0;
}