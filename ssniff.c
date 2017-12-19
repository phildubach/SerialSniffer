#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>

static uint64_t last_time = 0;

static int ser_open(char* dev_name)
{
    int fd = open(dev_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return fd;

    struct termios tset;
    tcgetattr(fd, &tset); /* read existing tset */

    cfsetispeed(&tset, B115200); // TODO: argument
    cfsetospeed(&tset, B115200); // TODO: argument
    tset.c_cflag &= ~CSIZE;
    tset.c_cflag |= CS8 | CLOCAL; // 8 bits
    tset.c_cflag &= ~PARENB;      // no parity
    tset.c_cflag &= ~CSTOPB;      // 1 stop bit
    //tset.c_lflag |= ICANON;
    tset.c_lflag &= ~ICANON;
    tset.c_lflag &= ~(IEXTEN | ISIG | ECHO);
    tset.c_iflag &= ~(ICRNL | INPCK | ISTRIP | IXON | BRKINT);
    tset.c_oflag &= ~OPOST;       // raw output

    tset.c_cc[VTIME]    = 2;      // inter-character timer unused
    //tset.c_cc[VMIN]     = 1;      // blocking read until 1 character arrives
    tset.c_cc[VMIN]     = 0;      // non-blocking

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &tset);
    // TODO: error handling

    return fd;
}

void ser_close(int fd)
{
    close(fd);
}

uint64_t time_get(void)
{
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    uint64_t time = spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
    uint64_t diff = time - last_time;
    last_time = time;
    return diff;
}

static struct line_buffer_s {
    char* pos;
    char buffer[1024];
} line_buffer[2];

static void queue_data(int port, char* buffer, int len)
{
    struct line_buffer_s* lb = &line_buffer[port];
    // TODO: check for buffer overflow
    while (len-- > 0)
    {
        char ch = *buffer++;
        if (ch == '\r')
        {
            *lb->pos++ = '\\';
            ch = 'r';
        }
        *lb->pos++ = ch;
        if (ch == '\n')
        {
            char header[20];
            sprintf(header, "%+10ld %c ", time_get(), port == 0 ? '<' : '>');
            write(STDOUT_FILENO, header, strlen(header));
            write(STDOUT_FILENO, lb->buffer, lb->pos - lb->buffer);
            lb->pos = lb->buffer;
        }
    }
}

void write_data(int fd, int port)
{
    char buffer[1024];
    int res = read(fd, buffer, sizeof(buffer));
    if ((res < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK))
    {
        perror("Cannot read()");
    }
    else if (res > 0)
    {
        queue_data(port, buffer, res);
    }
}

int main(int argc, char *argv[])
{
    char* port2 = "/dev/ttyUSB0";
    char* port1 = "/dev/ttyUSB1";

    int fd1 = ser_open(port1);
    if (fd1 < 0)
    {
        perror("Cannot open serial port");
    }
    else
    {
        printf("opened %s\n", port1);
    }

    int fd2 = ser_open(port2);
    if (fd2 < 0)
    {
        perror("Cannot open serial port");
    }
    else
    {
        printf("opened %s\n", port2);
    }

    time_get(); // initialize time

    // initialize line buffers
    line_buffer[0].pos = line_buffer[0].buffer;
    line_buffer[1].pos = line_buffer[1].buffer;

    /*
    fd_set port_set;
    while(true)
    {
        FD_ZERO(&port_set);
        FD_SET(fd1, &port_set);
        FD_SET(fd2, &port_set);
        int maxfd = fd1 > fd2 ? fd1 : fd2;
        if (select(maxfd + 1, &port_set, NULL, NULL, NULL) < 0)
        {
            if (errno == EINTR)
                continue;
            perror("Cannot select()");
        }
        if (FD_ISSET(fd1, &port_set))
        {
            write_data(fd1, 0);
        }
        if (FD_ISSET(fd2, &port_set))
        {
            write_data(fd2, 1);
        }
    }
    */
    while (true)
    {
        write_data(fd1, 0);
        write_data(fd2, 1);
    }

    ser_close(fd1);

    return 0;
}
