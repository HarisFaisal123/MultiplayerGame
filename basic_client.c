#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#define PORT 64472
#define SERVER_IP "127.0.0.1"  // Use "127.0.0.1" for localhost

int sockfd; // Declare sockfd globally for signal handler

// Signal handler function
void handle_signal(int signum) {
    printf("\nCtrl+C pressed. Closing the communication.\n");
    if (sockfd != -1) {
        close(sockfd); // Close the socket
    }
    exit(0); // Exit the program
}

int main() {
    struct sockaddr_in serv_addr;

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    // Set up the server address
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(1);
    // Set up the server address
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(1);
}
    printf("Connected to server\n");

    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_signal);

    // Send a message to the server
    char message[256];
    strcpy(message, "Hello, server!");
    if (send(sockfd, message, strlen(message), 0) < 0) {
        perror("Send failed");
        exit(1);
    }

    printf("Message sent to server: %s\n", message);
    for(;;){

    }

    // Close the socket (handled by signal handler)
    // close(sockfd);

    return 0;
}