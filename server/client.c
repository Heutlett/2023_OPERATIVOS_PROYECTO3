#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "colors.h"

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in servaddr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Configure server address
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
    {
        perror("Invalid server address");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        char message[BUFFER_SIZE];
        bold_green();
        printf("\nEnter a message to send to the server (or 'q')");
        default_color();
        printf("\n ━ ");
        fgets(message, BUFFER_SIZE, stdin);

        // Remove newline character from the input
        message[strcspn(message, "\n")] = '\0';

        if (strcmp(message, "q") == 0)
        {
            bold_yellow();
            printf("\n🔶 Client exiting...\n");
            default_color();
            break;
        }

        // Send the message to the server
        sendto(sockfd, (const char *)message, strlen(message), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));

        // Receive response from the server
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, NULL, NULL);
        buffer[n] = '\0'; // Null-terminate the received message

        bold_magenta();
        printf("   ● From server: %s\n", buffer);
        default_color();
    }

    close(sockfd);

    return 0;
}