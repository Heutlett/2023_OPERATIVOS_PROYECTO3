#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/**
 * The function attempts to open a file descriptor for a USB device and returns 0 if successful, 1
 * otherwise.
 *
 * @return The function `open_usb_fd()` returns an integer value. If the file descriptor `fd` is
 * successfully opened, the function returns 0. If there is an error opening the file descriptor, the
 * function returns -1.
 */
static int open_usb_fd()
{
    int fd;
    fd = open("/dev/ttyUSB0", O_RDWR);
    if (fd == -1)
    {
        perror("Error: Opening the driver file descriptor failed\n");
        close(fd);
        return fd;
    }
    return fd;
}

/**
 * The function writes data to a USB driver file and returns an error if the write operation fails.
 *
 * @param data A pointer to the data that needs to be written to the USB device.
 * @param data_len The length of the data to be written to the USB device.
 *
 * @return an integer value, either 0 or -1.
 */
static int write_to_usb(char *data, int data_len)
{
    int fd = open_usb_fd();
    ssize_t num_written;
    num_written = write(fd, data, data_len - 1);
    if (num_written == -1)
    {
        perror("Error: Writting to the driver file failed\n");
        close(fd);
        return num_written;
    }
    printf("\033[1;37m  ⭐ data sent  \033[0m");
    printf("\033[0;30m%s\033[0m\n", data);

    close(fd);
    return 0;
}

/**
 * The function "set_size" checks if the input parameter is valid and writes it to a USB if it is.
 *
 * @param size The parameter "size" is a pointer to a character array (string) that represents the size
 * of a physical symbols matrix. The function "set_size" takes this parameter and checks if it is equal to "s ", "m ",
 * or "b ". If it is, it calls the function "write_to_usb
 *
 * @return The function `set_size` returns an integer value. If the `size` parameter is equal to "s ",
 * "m ", or "b ", it calls the `write_to_usb` function with the `size` parameter and returns the value
 * returned by `write_to_usb`. If the `size` parameter is not equal to any of those values, it prints
 * an error message and returns -1
 */
int set_size(char *size)
{
    if (strcmp(size, "s") == 0 || strcmp(size, "m") == 0 || strcmp(size, "b") == 0)
    {
        return write_to_usb(size, 3);
    }
    else
    {
        printf("Error: Invalid size parameter\n");
        return -1;
    }
}

/**
 * The function "press_keys" writes a string of keys to a USB device by adding a prefix and suffix to
 * the string and then calling the "write_to_usb" function.
 *
 * @param keys The parameter "keys" is a pointer to a character array (string) that contains the keys
 * to be pressed.
 *
 * @return an integer value, which is the result of calling the function `write_to_usb()` with the
 * concatenated string `result` as the input parameter.
 */
int press_keys(char *keys)
{

    int res;

    char *prefix = "d ";
    char *suffix = "r ";

    size_t write_data_len = strlen(keys);
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);

    size_t result_len = write_data_len + prefix_len + suffix_len + 1; // +1 for the null terminator
    char *result = (char *)malloc(result_len);

    strcpy(result, prefix);
    strcat(result, keys);
    strcat(result, suffix);

    res = write_to_usb(result, result_len);

    free(result);

    return res;
}