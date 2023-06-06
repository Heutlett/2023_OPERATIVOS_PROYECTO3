#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "colors.h"
#include "utils.c"

#define BUFFER_SIZE 1024

char *extractDigits(const char *entry)
{
    char *message = (char *)malloc(BUFFER_SIZE * sizeof(char));
    int j = 0;

    for (int i = 0; entry[i] != '\0'; i++)
    {
        if (isdigit(entry[i]))
        {
            message[j] = entry[i];
            j++;
        }
    }
    return message;
}

/**
 * @brief Entry point of the UDP client program.
 * @param argc The number of command-line arguments.
 * @param argv An array of strings containing the command-line arguments.
 *             It should have four elements: <server_ip>, <port>, and <key>.
 * @return 0 on success, 1 on incorrect command-line arguments.
 */
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <server_ip> <port> <key>\n", argv[0]);
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

    char *code;
    char entry[BUFFER_SIZE];
    char *encrypted;

    while (1)
    {
        // Ask for input
        bold_green();
        printf("\n▶ Type your code: ");

        bold_white();
        fgets(entry, BUFFER_SIZE, stdin);
        code = extractDigits(entry);
        if (strlen(code) == 0)
        {
            continue;
        }

        bold_cyan();
        printf("   ◗ in  : %s\n", code);

        // XOR encrypt the message with the key
        encrypted = xorEncrypt(code, atoi(argv[3]));

        if (encrypted == NULL)
        {
            bold_red();
            printf("\n⛔ Encryption failed\n");
            default_color();
            continue;
        }

        bold_magenta();
        printf("   ◖ enc : %s\n", encrypted);
        default_color();

        // Send the message to the server
        sendto(sockfd, (const char *)encrypted, strlen(code), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    }

    close(sockfd);

    return 0;
}