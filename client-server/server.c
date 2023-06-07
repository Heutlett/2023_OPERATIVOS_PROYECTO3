#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include "colors.h"
#include "utils.c"

#define BUFFER_SIZE 1024

int sockfd;                     // Global socket file descriptor
struct sockaddr_in client_addr; // Global client address
int len;                        // Global address length

// sudo ufw allow 8080
// sudo ufw enable
// sudo ufw status

/**
 * @brief The function handles shutting down the server by closing the socket and printing a message.
 * @param sig The parameter "sig" is an integer representing the signal number that caused the function
 * to be called. In this case, the function "handle_shut_down" is designed to handle the signal for
 * shutting down the server.
 */
void handle_shut_down(int sig)
{
    // Close the socket
    close(sockfd);
    bold_yellow();
    printf("\nShutting down...\n");
    default_color();
    exit(EXIT_SUCCESS);
}

/**
 * @brief Creates and configures the upd server socket.
 * @param port The port number to bind the server socket to.
 */
void createServer(int port)
{
    int n;
    struct sockaddr_in server_addr;
    len = sizeof(client_addr);

    // Clean buffers
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
    {
        bold_red();
        printf("⛔ Socket creation failed.\n");
        exit(EXIT_FAILURE);
    }
    bold_blue();
    printf("\n🔘 Socket created");
    default_color();

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port); // Use the provided port

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        bold_red();
        printf("\n⛔ Binding failed.\n");
        exit(EXIT_FAILURE);
    }
    bold_white();
    printf("\n💬 Listening... %d\n", port); // Print the port
    default_color();
}

/**
 * @brief Handles incoming messages from clients.
 * @param key The key used for message decryption.
 */
void handleMessage()
{
    int n;
    char code[BUFFER_SIZE];
    char *decrypted;

    while (1)
    {

        // Receive client's message:
        n = recvfrom(sockfd, code, sizeof(code), 0, (struct sockaddr *)&client_addr, &len);
        code[n] = '\0'; // Null-terminate the received message
        if (n < 0)
        {
            bold_red();
            printf("\n⛔ Couldn't receive.\n");
            exit(EXIT_FAILURE);
        }

        bold_cyan();
        printf("\n[FROM CLIENT] ");
        bold_red();
        printf("encrypted ");
        bold_white();
        printf("- %s\n", addSpaces(code));
        default_color();

        // Fork a new process
        pid_t pid = fork();

        if (pid < 0)
        {
            printf("\n⛔ Couldn't create child process.\n");
            continue;
        }
        else if (pid == 0)
        {
            // Child process
            decrypted = addSpaces(rot128(code));

            if (decrypted == NULL)
            {
                printf("\n⛔ Decryption failed\n");
                exit(EXIT_FAILURE);
            }

            bold_cyan();
            printf("[PID : %d] ", getpid());
            bold_green();
            printf("decrypted ");
            bold_white();
            printf("- %s\n", decrypted);

            default_color();

            exit(EXIT_SUCCESS);
        }
        else
        {
            // Parent process
            continue;
        }
    }
}

/**
 * @brief Entry point of the UDP server program.
 * @param argc The number of command-line arguments.
 * @param argv An array of strings containing the command-line arguments.
 *             It should have two elements: <port>.
 * @return 0 on success, 1 on incorrect command-line arguments.
 */
int main(int argc, char *argv[])
{
    // Validate arguments
    if (argc != 2)
    {
        bold_yellow();
        printf("⭐ Usage: %s <port>\n", argv[0]);
        default_color();
        return 1;
    }

    // Handle termination
    signal(SIGINT, handle_shut_down);  // Set up a signal handler for Ctrl+C
    signal(SIGTSTP, handle_shut_down); // Set up a signal handler for Ctrl+Z

    // Create server and handle messages
    createServer(atoi(argv[1]));
    handleMessage();

    close(sockfd);

    return 0;
}