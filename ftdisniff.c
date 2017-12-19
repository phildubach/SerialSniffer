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
#include <stdlib.h>

#include <libftdi1/ftdi.h>

static uint64_t last_time = 0;

static struct ftdi_context *ftdi[2];

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

static void line_buffer_init(void)
{
    int i;
    for (i = 0; i < 2; i++)
    {
        line_buffer[i].pos = line_buffer[i].buffer;
    }
}

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
    int res, i;

    for (i = 0; i < 2; i++)
    {
        ftdi[i] = ftdi_new();
        if (ftdi[i] == NULL)
        {
            fprintf(stderr, "Cannot initialize ftdi library\n");
            exit(1);
        }

        res = ftdi_usb_open_desc_index(ftdi[i], 0x0403, 0x6001, NULL, NULL, i);
        if (res < 0)
        {
            fprintf(stderr, "Error opening ftdi device %d: %d (%s)\n", i, res, ftdi_get_error_string(ftdi[i]));
            exit(2);
        }

        res = ftdi_set_baudrate(ftdi[i], 115200);
        if (res < 0)
        {
            fprintf(stderr, "Error setting baudrate on ftdi device %d: %d (%s)\n", i, res, ftdi_get_error_string(ftdi[i]));
            exit(2);
        }

        res = ftdi_set_line_property(ftdi[i], BITS_8, STOP_BIT_1, NONE);
        if (res < 0)
        {
            fprintf(stderr, "Error setting baudrate on ftdi device %d: %d (%s)\n", i, res, ftdi_get_error_string(ftdi[i]));
            exit(2);
        }

        res = ftdi_setflowctrl(ftdi[i], SIO_DISABLE_FLOW_CTRL);
        if (res < 0)
        {
            fprintf(stderr, "Error setting flow control on ftdi device %d: %d (%s)\n", i, res, ftdi_get_error_string(ftdi[i]));
            exit(2);
        }

        res = ftdi_set_event_char(ftdi[i], '\n', 1);
        if (res < 0)
        {
            fprintf(stderr, "Error setting event char on ftdi device %d: %d (%s)\n", i, res, ftdi_get_error_string(ftdi[i]));
            exit(2);
        }

        res = ftdi_set_latency_timer(ftdi[i], 1);
        if (res < 0)
        {
            fprintf(stderr, "Error setting latency timer on ftdi device %d: %d (%s)\n", i, res, ftdi_get_error_string(ftdi[i]));
            exit(2);
        }

        res = ftdi_read_data_set_chunksize(ftdi[i], 1024);
        if (res < 0)
        {
            fprintf(stderr, "Error setting chunk size on ftdi device %d: %d (%s)\n", i, res, ftdi_get_error_string(ftdi[i]));
            exit(2);
        }
    }

    time_get();
    line_buffer_init();

    while (true)
    {
        for (i = 0; i < 2; i++)
        {
            char buffer[1024];
            res = ftdi_read_data(ftdi[i], buffer, sizeof(buffer));
            if (res > 0)
            {
                queue_data(i, buffer, res);
            }
        }
    }

    for (i = 0; i < 2; i++)
    {
        if (ftdi[i] != NULL)
        {
            res = ftdi_usb_close(ftdi[i]);
            if (res < 0)
            {
                fprintf(stderr, "Error closing ftdi device %d: %d (%s)\n", i, res, ftdi_get_error_string(ftdi[i]));
                exit(2);
            }
            ftdi_free(ftdi[i]);
        }
    }

    return 0;
}
