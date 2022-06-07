#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
using namespace std;

// client side
int main(int argc, char *argv[]){
    // we need 2 things: ip address and port number, in that order
    if(argc != 3) {
        cerr << "Usage: ip_address port" << endl; exit(0); 
    }

    // grab the IP address and port number
    char *serverIp = argv[1];
    int port = atoi(argv[2]); 

    // create a message buffer
    char msg[4096];

    // setup a socket and connection tools 
    struct hostent* host = gethostbyname(serverIp);
    sockaddr_in sendSockAddr;
    bzero((char*) &sendSockAddr, sizeof(sendSockAddr));
    sendSockAddr.sin_family = AF_INET; 
    sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(port);
    int clientSd = socket(AF_INET, SOCK_STREAM, 0);

    // try to connect...
    int status = connect(clientSd, (sockaddr*) &sendSockAddr, sizeof(sendSockAddr));

    if(status < 0)
        cout << "Error connecting to socket! " << endl;

    cout << "Connected to the server!" << endl;

    int bytesRead = 0, bytesWritten = 0;
    struct timeval start1, end1;
    gettimeofday(&start1, NULL);

    while(1) {
        cout << ">";

        // reading stdin input
        string data;
        getline(cin, data);

        // clear the buffer
        memset(&msg, 0, sizeof(msg));
        strcpy(msg, data.c_str());

        // checks for the exit statement
        if(data == "exit") {
            send(clientSd, (char*)&msg, strlen(msg), 0);
            break;
        }

        // counting the bytes
        bytesWritten += send(clientSd, (char*) &msg, strlen(msg), 0);
        cout << "Awaiting server response..." << endl;

        // clear the buffer
        memset(&msg, 0, sizeof(msg));
        bytesRead += recv(clientSd, (char*) &msg, sizeof(msg), 0);

        // checks if the server has closed the connection
        if(!strcmp(msg, "exit")) {
            cout << "Server has quit the session" << endl;
            break;
        }

        cout << "Server: " << msg << endl;
    }

    gettimeofday(&end1, NULL);
    close(clientSd);
    cout << endl << "========Session========" << endl;
    cout << "Bytes written: " << bytesWritten << endl << " Bytes read: " << bytesRead << endl;
    cout << "Elapsed time: " << (end1.tv_sec - start1.tv_sec) << " secs" << endl;
    cout << "Connection closed" << endl;
    return 0;
}