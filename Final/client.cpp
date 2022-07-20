#include <iostream>
#include <pthread.h>
#include <signal.h>
#include "client.hpp"
#include "socket.hpp"

using namespace std;

char nickname[MESSAGE_SIZE] = {};
char channel[CHANNEL_NAME_SIZE] = {};

/* Prints an error message and closes socket connection in case of failure */
void errorMessage(int sock, const char *message) {
    perror(message);
    close(sock);
    exit(1);
}

/* Closes socket connection */
void quit(int sock) {
    cout << "Quitting" << endl;
    close(sock);
    exit(0);
}

/* Ignores default crtlCHandler */
void ctrlCHandler(int signal) {
    cout << "To exit the program use /q" << endl;
}

/* Prints user nickname */
void printNickname() {
    cout << "\r" << channel << " - " << nickname << endl;
    fflush(stdout);
}

/* Add a newChar to the end of a string */
void addEndStr(char *str, char newChar) {
    int size = strlen(str);

    if (newChar == str[size - 1]) return;
    if ((isalnum(str[size - 1])) || ispunct(str[size - 1])) str[size] = newChar;
    else str[size - 1] = newChar;
}

/* Thread function for receiving server messages */
void *receiveMessageHandler(void *sock) {
    char buffer[MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5] = {};
    int rcv;
    while (true) {
        rcv = recv(*(int *)sock, buffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5, 0);
        
        // If user has lost server connection
        if (rcv == 0) {
            cout << "\rLost connection to server..." << endl;
            fflush(stdout);
        
            quit(*(int *)sock);
        }
        
        // If there is any error reading the message
        if (rcv < 0) errorMessage(*(int *)sock, "ERROR reading from sock");
        
        // If user has just changed chanels (server sends a 'nofication' if operation was sucessfull)
        if (buffer[0] == '/') {
            char message[MESSAGE_SIZE];
            sscanf(buffer, "/channel %s %[^\n]", channel, message);
            cout << "\r" << message << " " << channel;
            printf("%*c\n", 20, ' ');
            fflush(stdout);

            cout << "\r";
            fflush(stdout);
            printNickname();
        } else {
            addEndStr(buffer, '\0');
            cout << "\r" << buffer;
            printf("%*c\n", 20, ' ');
            fflush(stdout);
            printNickname();
        }

        // Confirms if the message has been received
        bzero(buffer, MESSAGE_SIZE + NICKNAME_SIZE + CHANNEL_NAME_SIZE + 5);
        strcpy(buffer, "/ack");
        
        if (send(*(int *)sock, buffer, strlen(buffer), MSG_DONTWAIT) < 0) 
            errorMessage(*(int *)sock, "ERROR writing to sock");
    }
}

/* Thread function for sending messages to the server */
void *sendMessageHandler(void *sock) {
    char buffer[MESSAGE_SIZE] = {};
    string message;
    int snd;
    while (true) {
        printNickname();
        getline(cin, message);
        int div = (message.length() > (MESSAGE_SIZE - 1)) ? (message.length() / (MESSAGE_SIZE - 1)) : 0;
    	
        // Sends messages in blocks if necessary
        for (int i = 0; i <= div; i++) {
            bzero(buffer, MESSAGE_SIZE);
            message.copy(buffer, MESSAGE_SIZE - 1, (i * (MESSAGE_SIZE - 1)));

            if (buffer[0] != '/') addEndStr(buffer, '\n');
            
            if (send(*(int *)sock, buffer, strlen(buffer), MSG_DONTWAIT) < 0)
                errorMessage(*(int *)sock, "ERROR writing to sock");
        }

        // Disconnects user (after notifying server)
        if (strcmp(buffer, "/q") == 0) quit(*(int *)sock);
    }
}

int main() {
    struct sockaddr_in serverAddr, clientAddr;
    int serverAddrLen = sizeof(serverAddr);
    int clientAddrLen = sizeof(clientAddr);
    int sock = 0;
    char ip[INET6_ADDRSTRLEN] = {};
    char buffer[MESSAGE_SIZE] = {};
    char command[MESSAGE_SIZE] = {};

    strcpy(channel, "#none");
    bzero(nickname, NICKNAME_SIZE);
    bzero(buffer, MESSAGE_SIZE);
    bzero((char *)&clientAddr, clientAddrLen);
    bzero((char *)&serverAddr, serverAddrLen);

    // Asks user to set nickname
    do {
        cout << "Set your nickname (1~50 chars)" << endl << "Usage: /user <username>" << endl;
        if (fgets(buffer, MESSAGE_SIZE - 1, stdin) != NULL && buffer[0] == '/') {
            sscanf(buffer, "%s %s", command, nickname);
            
            if (strcmp(command, "/user") == 0) addEndStr(nickname, '\0');
            else cout << "Invalid command" << endl;
        }
    } while(strlen(nickname) < 1 || strlen(nickname) > NICKNAME_SIZE - 1);

    // Configures connection 
    if ((sock = socket(AF_INET, SOCK_STREAM, PROTOCOL)) < 0)
        errorMessage(sock, "\nSocket creation error\n");
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    int addr = 1;
    
    do {
        if (addr <= 0) cout << "Invalid address. ";

        cout << "Please enter server address (default: " << LOCALHOST << ")." << endl << "For default enter '/': ";

        if (fgets(ip, MESSAGE_SIZE - 1, stdin) != NULL) addEndStr(ip, '\0');
        if (!strcmp(ip, "/")) addr = inet_pton(AF_INET, LOCALHOST, &serverAddr.sin_addr);
        else addr = inet_pton(AF_INET, ip, &serverAddr.sin_addr);

    } while(addr <= 0);

    signal(SIGINT, ctrlCHandler);

    // User now can either connect with the server or quit
    cout << endl << "Connect: /c" << endl << "Quit: /q" << endl;
    while (true) {
        if (fgets(buffer, 12, stdin) != NULL) addEndStr(buffer, '\0');

        if (!strcmp(buffer, "/c")) break;
        else if (!strcmp(buffer, "/q")) quit(sock);
        else cout << "Unkown command" << endl;
    }

    // If user proceeds with connection
    if (connect(sock, (struct sockaddr *)&serverAddr, serverAddrLen) < 0)
        errorMessage(sock, "\nConnection failed");

    getsockname(sock, (struct sockaddr *)&clientAddr, (socklen_t *)&clientAddrLen);

    clientAddr.sin_port = htons(PORT);
    cout << "Connect to Server: " << inet_ntoa(serverAddr.sin_addr) << ": " << ntohs(serverAddr.sin_port) << endl;
    cout << "You are: " << inet_ntoa(clientAddr.sin_addr) << ": " << ntohs(clientAddr.sin_port) << endl;
    
    send(sock, nickname, NICKNAME_SIZE, 0);

    // After connection, starts user threads (receive and send)
    pthread_t receiveMessageThread;
    if (pthread_create(&receiveMessageThread, NULL, receiveMessageHandler, &sock) != 0)
        errorMessage(sock, "\nCreate pthread error");
    
    pthread_t sendMessageThread;
    if (pthread_create(&sendMessageThread, NULL, sendMessageHandler, &sock) != 0)
        errorMessage(sock, "\nCreate pthread error");

    pthread_join(receiveMessageThread, NULL);
    pthread_join(sendMessageThread, NULL);

    return 0;
}