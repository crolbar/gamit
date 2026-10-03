#pragma once

// char[]
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#define MSG_STRING 0

// int32
// treat -1 in data as a null terminator
#define MSG_DATA 1

#define MSG_PLAYER_ID  1
#define MSG_PLAYER_POS 2

struct player
{
    int id;
    int fd;
    int x;
    int y;
};

static int
send_msg_s(int fd, char* buf)
{
    size_t body_size = strlen(buf);
    int    n =
      write(fd, (uint32_t[]){ MSG_STRING, body_size }, sizeof(uint32_t) * 2);
    if (n < 0)
        return 1;

    n = write(fd, buf, body_size);
    if (n < 0)
        return 1;
    return 0;
}

static int
send_msg_d(int fd, int32_t* data, size_t data_len)
{
    size_t body_size = sizeof(int32_t) * data_len;
    int    n =
      write(fd, (uint32_t[]){ MSG_DATA, body_size }, sizeof(uint32_t) * 2);
    if (n < 0)
        return 1;

    n = write(fd, data, body_size);
    if (n < 0)
        return 1;
    return 0;
}
