#include <iostream>
#include <pthread.h>
#include <signal.h>
#include "client.hpp"
#include "socket.hpp"

using namespace std;

char nickname[MESSAGE_SIZE] = {};
char channel[CHANNEL_NAME_SIZE] = {};

void errorMessage(const char *message) {
    perror(message);
    exit(1);
}

void quit(int sock) {
    close(sock);
    exit(0);
}

void ctrlCHandler(int signal) {
    cout << "To exit the program use /quit" << endl;
}

void printNickname() {
    cout << "\r" << channel << " - " << nickname << endl;
    fflush(stdout);
}

void strTrim(char *str, char newChar) {
    int size = strlen(str);

    if (newChar == str[size - 1])
        return;
    
    if ((isalnum(str[size - 1])) || ispunct(str[size - 1]))
        str[size] = newChar;
    else 
        str[size - 1] = newChar;
}

void *receiveMessageHandler(void *sock) {
    char buffer[MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5] = {};
    int rcv;
    while (true) {
        rcv = recv(*(int *)sock, buffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5, 0);
        if (rcv == 0) {
            cout << "\rLost connection to server..." << endl;
            fflush(stdout);
            quit(*(int *)sock);
        } else if (rcv < 0) {
            errorMessage("ERROR reading from sock");
        } else if (buffer[0] == '/') {
            char message[MESSAGE_SIZE];
            sscanf(buffer, "/channel %s %[^\n]", channel, message);
            cout << "\r" << message << " " << channel;
            printf("%*c\n", 20, ' ');
            fflush(stdout);

            cout << "\r";
            fflush(stdout);
            printNickname();

            bzero(buffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5);
            strcpy(buffer, "/ack");
            int snd = send(*(int *)sock, buffer, strlen(buffer), MSG_DONTWAIT);
            if (snd < 0)
                errorMessage("ERROR writing to sock");
        } else {
            strTrim(buffer, '\0');
            cout << "\r" << buffer;
            printf("%*c\n", 20, ' ');
            fflush(stdout);
            printNickname();

            bzero(buffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5);
            strcpy(buffer, "/ack");
            int snd = send(*(int *)sock, buffer, strlen(buffer), MSG_DONTWAIT);
            if (snd < 0)
                errorMessage("ERROR writing to sock");
        }
    }
}

void *sendMessageHandler(void *sock) {
    char buffer[MESSAGE_SIZE] = {};
    string message;
    int snd;
    while (true) {
        printNickname();
        getline(cin, message);
        int div = (message.length() > (MESSAGE_SIZE - 1)) ? (message.length() / (MESSAGE_SIZE - 1)) : 0;

        for (int i = 0; i <= div; i++) {
            bzero(buffer, MESSAGE_SIZE);

            message.copy(buffer, MESSAGE_SIZE - 1, (i * (MESSAGE_SIZE - 1)));

            if (buffer[0] != '/')
                strTrim(buffer, '\n');
            int snd = send(*(int *)sock, buffer, strlen(buffer), MSG_DONTWAIT);
            if (snd < 0)
                errorMessage("ERROR writing to sock");
        }

        if (!strcmp(buffer, "/quit")) {
            cout << "Quitting" << endl;
            quit(*(int *)sock);
        }
    }
}

int main() {
    struct sockaddr_in serverAddr, clientAddr;
    int serverAddrLen = sizeof(serverAddr);
    int clientAddrLen = sizeof(clientAddr);
    int sock = 0;
    char buffer[MESSAGE_SIZE] = {};
    char command[MESSAGE_SIZE] = {};

    signal(SIGINT, ctrlCHandler);
    bzero(nickname, NICKNAME_SIZE);
    strcpy(channel, "#none");

    do {
        cout << "Set your nickname (1~50 chars)" << endl << "Usage: /nickname <username>" << endl;
        if (fgets(buffer, MESSAGE_SIZE - 1, stdin) != NULL) {
            if (buffer[0] == '/') {
                sscanf(buffer, "%s %s", command, nickname);
                if (!strcmp(command, "/nickname"))
                    strTrim(nickname, '\0');
                else
                    cout << "Invalid command" << endl;
            }
        }
    } while(strlen(nickname) < 1 || strlen(nickname) > NICKNAME_SIZE - 1);

    bzero(buffer, MESSAGE_SIZE);

    if ((sock = socket(AF_INET, SOCK_STREAM, PROTOCOL)) < 0)
        errorMessage("\nSocket creation error\n");

    bzero((char *)&serverAddr, serverAddrLen);
    bzero((char *)&clientAddr, clientAddrLen);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    char ip[INET6_ADDRSTRLEN] = {};

    int addr = 1;
    do {
        if (addr <= 0)
            cout << "Invalid address. ";

        cout << "Please enter server address (default: " << LOCALHOST << ")." << endl << "For default enter /default: ";

        if (fgets(ip, MESSAGE_SIZE - 1, stdin) != NULL)
            strTrim(ip, '\0');

        if (!strcmp(ip, "/default"))
            addr = inet_pton(AF_INET, LOCALHOST, &serverAddr.sin_addr);
        else
            addr = inet_pton(AF_INET, ip, &serverAddr.sin_addr);

    } while(addr <= 0);

    cout << "Connect: /connect" << endl << "Quit: /quit" << endl << "Ping: /ping" << endl;
    bool connected = false;
    while (!connected) {
        if (fgets(buffer, 12, stdin) != NULL)
            strTrim(buffer, '\0');

        if (!strcmp(buffer, "/connect"))
            connected = true;
        else if (!strcmp(buffer, "/quit")) {
            cout << "Quitting" << endl;
            quit(sock);
        } else if (!strcmp(buffer, "/ping"))
            cout << "/ping can only be used after connecting to server" << endl;
        else
            cout << "Unkown command" << endl;
    }

    if (connect(sock, (struct sockaddr *)&serverAddr, serverAddrLen) < 0)
        errorMessage("\nConnection failed\n");

    getsockname(sock, (struct sockaddr *)&clientAddr, (socklen_t *)&clientAddrLen);

    clientAddr.sin_port = htons(PORT);
    cout << "Connect to Server: " << inet_ntoa(serverAddr.sin_addr) << ": " << ntohs(serverAddr.sin_port) << endl;
    cout << "You are: " << inet_ntoa(clientAddr.sin_addr) << ": " << ntohs(clientAddr.sin_port) << endl;

    send(sock, nickname, NICKNAME_SIZE, 0);

    pthread_t receiveMessageThread;
    if (pthread_create(&receiveMessageThread, NULL, receiveMessageHandler, &sock) != 0)
        errorMessage("\nCreate pthread error\n");
    
    pthread_t sendMessageThread;
    if (pthread_create(&sendMessageThread, NULL, sendMessageHandler, &sock) != 0)
        errorMessage("\nCreate pthread error\n");

    pthread_join(receiveMessageThread, NULL);
    pthread_join(sendMessageThread, NULL);

    return 0;
}