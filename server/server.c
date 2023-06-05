#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "colors.h"
#include <signal.h>

#define BUFFER_SIZE 1024

int sockfd;                     // Global socket file descriptor
struct sockaddr_in client_addr; // Global client address
int len;                        // Global address length

// sudo ufw allow 8080
// sudo ufw enable
// sudo ufw status

/**
 * The function handles shutting down the server by closing the socket and printing a message.
 *
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

// UDP Server :
//    1.  Create a UDP socket.
//    2.  Bind the socket to the server address.
//    3.  Wait until the datagram packet arrives from the client.
//    4.  Process the datagram packet and send a reply to the client.
//    5.  Go back to Step 3.
void createServer(int port)
{
    int n;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
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

void handleMessage()
{
    int n;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];

    while (1)
    {
        // Receive client's message:
        n = recvfrom(sockfd, client_message, sizeof(client_message), 0, (struct sockaddr *)&client_addr, &len);
        client_message[n] = '\0'; // Null-terminate the received message
        if (n < 0)
        {
            bold_red();
            printf("\n⛔ Couldn't receive.\n");
            exit(EXIT_FAILURE);
        }

        bold_green();
        printf("\n   ● From client: %s\n", client_message);
        default_color();

        // Process the message (You can add your own logic here)
        // For now it's just copying the input to the output
        strcpy(server_message, client_message);

        // Send a response to the client
        if (sendto(sockfd, server_message, strlen(server_message), 0, (struct sockaddr *)&client_addr, len) < 0)
        {
            bold_red();
            printf("\n⛔ Couldn't send.\n");
            default_color();

            exit(EXIT_FAILURE);
        }

        bold_magenta();
        printf("      ↪ To: %s\n", server_message);
        default_color();
    }
}

int main(int argc, char *argv[])
{
    // Validate arguments
    if (argc != 2)
    {
        bold_yellow();
        printf("⭐ Usage: %s <port> <ip>\n", argv[0]);
        default_color();
        return 1;
    }

    // Get the parameters from the server and store them in global variables
    int port = atoi(argv[1]);

    // Handle termination
    signal(SIGINT, handle_shut_down);  // Set up a signal handler for Ctrl+C
    signal(SIGTSTP, handle_shut_down); // Set up a signal handler for Ctrl+Z

    // Create server and handle messages
    createServer(port);
    handleMessage();

    close(sockfd);

    return 0;
}