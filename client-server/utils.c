#include <stdlib.h> // for dynamic memory allocation
#include <string.h>
#include <errno.h>

/**
 * @brief Performs XOR encryption on the given message using the provided key.
 *
 * @param message The message to be encrypted.
 * @param key The key used for encryption.
 * @return The encrypted message as a dynamically allocated string.
 *         If memory allocation fails or input parameters are invalid, returns NULL.
 *         It is the responsibility of the caller to free the memory.
 */
char *xorEncrypt(const char *message, int key)
{
    if (message == NULL || key < 0 || key > 255)
    {
        bold_red();
        printf("\n⛔  Invalid key range between 0 - 255.\n");
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
        encrypted[i] = message[i] ^ key;
    }
    encrypted[messageLen] = '\0'; // Null-terminate the encrypted message

    return encrypted;
}
