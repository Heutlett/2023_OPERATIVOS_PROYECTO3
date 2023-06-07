#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <my_lib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "colors.h"
#include "utils.c"

/* *********************************
    Variables and Constants
************************************ */

#define BUFFER_SIZE 1024 // Buffer size constant

int sockfd;                     // Global socket file descriptor
struct sockaddr_in client_addr; // Global client address
int len;                        // Global address length

sem_t *sem_mutex;                    // Semaphore instance
const char *sem_mutex_name = "/sem"; // Semaphore instance names

// sudo ufw allow 8080
// sudo ufw enable
// sudo ufw status

/* *********************************
    Functions
************************************ */

char *extractDigits(const char *entry)
{
    char *message = (char *)malloc(BUFFER_SIZE * sizeof(char));
    int j = 0;

    for (int i = 0; entry[i] != '\0'; i++)
    {
        if (isdigit(entry[i]) || entry[i] == ' ')
        {
            message[j] = entry[i];
            j++;
        }
    }

    return message;
}

char *extractLetters(const char *entry)
{
    char *message = (char *)malloc(BUFFER_SIZE * sizeof(char));
    int j = 0;

    for (int i = 0; entry[i] != '\0'; i++)
    {
        if (isalpha(entry[i]))
        {
            message[j] = entry[i];
            j++;
        }
    }

    return message;
}

/**
 * The function creates semaphores with specific names and permissions.
 */
void create_semaphore()
{
    sem_mutex = sem_open(sem_mutex_name, O_CREAT, S_IRUSR | S_IWUSR, 1);
    if (sem_mutex == SEM_FAILED)
    {
        bold_red();
        perror("sem_open() failed");
        default_color();
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief The function handles shutting down the server by closing the socket and printing a message.
 * @param sig The parameter "sig" is an integer representing the signal number that caused the function
 * to be called. In this case, the function "handle_shut_down" is designed to handle the signal for
 * shutting down the server.
 */
void handle_shut_down(int sig)
{

    // Destroy semaphore
    sem_close(sem_mutex);
    sem_unlink(sem_mutex_name);

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

        // bold_cyan();
        // printf("\n[FROM CLIENT] ");
        // bold_red();
        // printf("encrypted ");
        // bold_white();
        // printf("- %s\n", addSpaces(code));
        // default_color();

        // Fork a new process
        pid_t pid = fork();

        if (pid < 0)
        {
            printf("\n⛔ Couldn't create child process.\n");
            continue;
        }
        else if (pid == 0)
        {

            int result;

            // Child process
            decrypted = addSpaces(rot128(code));

            if (decrypted == NULL)
            {
                printf("\n⛔ Decryption failed\n");
                exit(EXIT_FAILURE);
            }

            bold_cyan();
            printf("\n[PID : %d] ", getpid());
            bold_green();
            printf("decrypted ");
            bold_white();
            printf("- %s\n", decrypted);
            default_color();

            // Take care of the resource
            sem_wait(sem_mutex);

            bold_cyan();
            printf("[PID : %d] ", getpid());
            bold_magenta();
            printf("acquired\n");
            default_color();

            char *size;

            size = extractLetters(decrypted);

            printf(size);
            printf("%d\n", strlen(size));
            printf("%d\n", strcmp(size, "s"));

            if (strcmp(size, "s") == 0 || strcmp(size, "m") == 0 || strcmp(size, "b") == 0)
            {
                result = set_size(size);
            }
            else
            {
                decrypted = extractDigits(decrypted);
                result = press_keys(decrypted);
            }
            if (result < 0)
            {
                bold_cyan();
                printf("[PID : %d] ", getpid());
                bold_yellow();
                printf("write ");
                bold_white();
                printf("- failed\n");
                default_color();
            }

            else
            {
                bold_cyan();
                printf("[PID : %d] ", getpid());
                bold_yellow();
                printf("write ");
                bold_white();
                printf("- succesfull\n");
                default_color();
            }

            bold_cyan();
            printf("[PID : %d] ", getpid());
            bold_red();
            printf("awaiting processing...\n");
            default_color();

            sleep(5); // Wait 10 segundos

            bold_cyan();
            printf("[PID : %d] ", getpid());
            bold_magenta();
            printf("released\n");
            default_color();

            sem_post(sem_mutex);

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

    // Create semaphore
    create_semaphore();

    // Handle termination
    signal(SIGINT, handle_shut_down);  // Set up a signal handler for Ctrl+C
    signal(SIGTSTP, handle_shut_down); // Set up a signal handler for Ctrl+Z

    // Create server and handle messages
    createServer(atoi(argv[1]));
    handleMessage();

    // Close socket
    close(sockfd);

    // Destroy semaphore
    sem_close(sem_mutex);
    sem_unlink(sem_mutex_name);

    return 0;
}