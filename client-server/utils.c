#include <stdlib.h> // for dynamic memory allocation
#include <string.h>
#include <stdio.h>

#define SEPARATOR ' '

/**
 * @brief Performs ROT128 encryption on the given message.
 *
 * @param message The message to be encrypted.
 * @return The encrypted message as a dynamically allocated string.
 *         If memory allocation fails or the input parameter is invalid (NULL), returns NULL.
 *         It is the responsibility of the caller to free the memory.
 */
char *rot128(const char *message)
{
    if (message == NULL)
    {
        bold_red();
        printf("\n⛔  Invalid message.\n");
        exit(EXIT_FAILURE);
    }

    size_t messageLen = strlen(message);
    char *encrypted = (char *)malloc(messageLen + 1); // +1 for null terminator

    if (encrypted == NULL)
    {
        bold_red();
        printf("\n⛔  Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < messageLen; ++i)
    {
        encrypted[i] = (message[i] + 128) % 256;
    }
    encrypted[messageLen] = '\0'; // Null-terminate the encrypted message

    return encrypted;
}

char *addSpaces(const char *input)
{
    // Get the length of the input string
    size_t len = strlen(input);

    // Create a new string with double the length to add spaces
    char *output = (char *)malloc((2 * len + 1) * sizeof(char));

    // Index to iterate through the input string
    int i = 0;

    // Index to iterate through the output string
    int j = 0;

    // Copy each character from the input string to the output string
    // and add a space after each character
    while (input[i] != '\0')
    {
        output[j] = input[i];
        output[j + 1] = SEPARATOR;
        i++;
        j += 2;
    }

    // Add the null character at the end of the output string
    // output[j] = '\0';

    return output;
}