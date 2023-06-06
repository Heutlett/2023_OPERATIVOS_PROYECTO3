#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd = open("/dev/mydriver", O_RDWR);
    if (fd < 0)
    {
        perror("Failed to open the device file");
        return -1;
    }

    // Read from or write to the device using the `read` and `write` functions
    // ...

    close(fd);
    return 0;
}