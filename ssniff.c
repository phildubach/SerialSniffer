#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

static int ser_open(char* dev_name)
{
    int fd = open(dev_name, O_RDWR | O_NONBLOCK);    
    if (fd < 0) return fd;

    struct termios tset;
    tcgetattr(fd, &tset); /* read existing tset */

    cfsetspeed(&tset, B115200); // TODO: argument
    tset.c_cflag &= ~CSIZE;
    tset.c_cflag |= CS8 | CLOCAL; // 8 bits
    tset.c_cflag &= ~PARENB;      // no parity
    tset.c_cflag &= ~CSTOPB;      // 1 stop bit
    tset.c_lflag = ICANON;        // canonical mode
    tset.c_oflag &= ~OPOST;       // raw output

    tcsetattr(fd, TCSANOW, &tset);
    tcflush(fd, TCOFLUSH);
    // TODO: error handling

    return fd;
}

void ser_close(int fd)
{
    close(fd);
}

int main(int argc, char *argv[])
{
    char* port2 = "/dev/tty.usbserial-A50285BI";
    char* port1 = "/dev/tty.usbserial-A50285BI10";

    int fd1 = ser_open(port1);
    if (fd1 < 0)
    {
        perror("Cannot open serial port");
    }
    else
    {
        printf("opened %s\n", port1);
    }

    fd_set port_set;
    while(true)
    {
        FD_ZERO(&port_set);
        FD_SET(fd1, &port_set);
        if (select(fd1 + 1, &port_set, NULL, NULL, NULL) < 0)
        {
            if (errno == EINTR)
                continue;
            perror("Cannot select()");
        }
        if (FD_ISSET(fd1, &port_set))
        {
            char buffer[1024];
            int res = read(fd1, buffer, sizeof(buffer));
            if (res < 0)
            {
                perror("Cannot read()");
            }
            else if (res > 0)
            {
                write(STDOUT_FILENO, buffer, res);
            }
        }
    }

    ser_close(fd1);

    return 0;
}
