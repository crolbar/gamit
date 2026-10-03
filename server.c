#include "dll.h"
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "game.h"

#define PORT        6767
#define PFDS        1
#define MAX_CLIENTS 10

static dll(struct player) players;

int
send_update(int fd)
{
    // printf("players\n");
    // dll_for_each(players, v)
    // {
    //     printf("%d, %d, %d\n", v->val.id, v->val.x, v->val.y);
    // }
    // printf("\n");

    dll_for_each(players, v)
    {
        dll_for_each(players, rv)
        {
            if (v->val.id == rv->val.id)
                continue;

            int n = send_msg_d(rv->val.fd,
                               (int32_t[]){ MSG_PLAYER_ID,
                                            v->val.id,
                                            MSG_PLAYER_POS,
                                            v->val.x,
                                            v->val.y,
                                            -1 },
                               6);
        }
    }

    return 0;
}

int
main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("ipc bind failed\n");
        goto fail;
    }

    if (listen(sockfd, 10) == -1) {
        printf("ipc listen failed: %s\n", strerror(errno));
        goto fail;
    }

    printf("Listening on :%d\n", PORT);

    int            player_id = 1;
    struct pollfd* pfds      = (struct pollfd[PFDS + MAX_CLIENTS]){
        { .fd = sockfd, .events = POLLIN },
    };
    for (int i = PFDS; i < PFDS + MAX_CLIENTS; i++) {
        pfds[i] = (struct pollfd){ .fd = -1, .events = POLLIN };
    }

    players = (typeof(players))dll_init();

    while (1) {
        int n = poll(pfds, PFDS + MAX_CLIENTS, 0);
        if (n < 0) {
            printf("err on poll: %s", strerror(errno));
            goto fail;
        }

        if (pfds[0].revents & POLLIN) {
            int clientfd = accept(sockfd, NULL, NULL);
            if (clientfd == -1) {
                printf("connection closed\n");
                continue;
            }
            printf("connection in\n");

            // pfds[1 + clients_n].fd = clientfd;
            for (int i = PFDS; i < PFDS + MAX_CLIENTS; i++) {
                if (pfds[i].fd == -1) {
                    pfds[i].fd = clientfd;
                    break;
                }
            }

            int  id      = player_id++;
            char buf[32] = { 0 };
            sprintf(buf, "%d", id);
            int n = write(clientfd, buf, strlen(buf));

            // players
            struct player p = { .fd = clientfd, .id = id, .x = 0, .y = 0 };
            dll_push_tail(players, p);
        }

        for (int i = PFDS; i < PFDS + MAX_CLIENTS; i++) {
            if (pfds[i].fd == -1)
                continue;
            if (!(pfds[i].revents & POLLIN))
                continue;

            int fd = pfds[i].fd;

            uint32_t msg_head[2] = { 0 };
            ssize_t  n           = recv(fd, msg_head, sizeof(uint32_t) * 2, 0);
            if (n <= 0) {
                goto close_conn;
            }

            printf("msg %d (size %d) | ", msg_head[0], msg_head[1]);

            if (msg_head[0] == MSG_STRING) {
                char buf[msg_head[1]];
                memset(buf, 0, msg_head[1]);
                n = recv(fd, buf, msg_head[1], 0);
                if (n <= 0) {
                    goto close_conn;
                }

                buf[n] = '\0';
                printf("red: %s\n", buf);
            } else if (msg_head[0] == MSG_DATA) {
                int32_t buf[msg_head[1]];
                memset(buf, 0, msg_head[1]);
                n = recv(fd, buf, msg_head[1], 0);
                if (n <= 0) {
                    goto close_conn;
                }

                int pid = -1;

                for (int i = 0; buf[i] != -1; i++) {
                    printf("%d ", buf[i]);
                }
                printf("\n");

                for (int i = 0; buf[i] != -1; i++) {
                    // printf("msg field: %d\n", buf[i]);

                    if (buf[i] == MSG_PLAYER_ID) {
                        pid = buf[i + 1];
                        i += 1;
                    } else if (buf[i] == MSG_PLAYER_POS) {
                        if (pid == -1) {
                            printf("requested pos up, with no id provided\n");
                            continue;
                        }

                        dll_for_each(players, v)
                        {
                            if (pid != v->val.id)
                                continue;

                            v->val.x = buf[i + 1];
                            v->val.y = buf[i + 2];
                        }

                        i += 2;
                    }
                }
                if (pid == -1)
                    continue;

                send_update(sockfd);
            }

            continue;
        close_conn:
            printf("connection closed\n");
            dll_for_each(players, v)
            {
                if (v->val.fd != fd)
                    continue;
                dll_remove(players, v);
                break;
            }
            close(fd);
            pfds[i].fd      = -1;
            pfds[i].revents = 0;
        }
    }

    return 0;
fail:
    return 1;
}
