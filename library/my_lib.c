#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 2
int main()
{
    int fd;
    char write_data[] = "F100";
    ssize_t num_written, num_read;

    // Abrir el dispositivo FT232RL USB
    fd = open("/dev/ttyUSB0", O_RDWR);
    if (fd == -1) {
        perror("Error al abrir el dispositivo");
        close(fd);
        return 1;
    }
    num_written = write(fd, write_data, sizeof(write_data) - 1);
    if (num_written == -1) {
        perror("Error al escribir en el dispositivo");
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}