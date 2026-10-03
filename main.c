#include "dll.h"
#include "game.h"
#include <errno.h>
#include <netinet/in.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 6767

#define PLAYER_WIDTH  50
#define PLAYER_HEIGHT 50
#define PLAYER_SPEED  5
#define PROJ_SIZE     100
#define PROJ_SPEED    5

static dll(struct player) players;

void
DrawPlayer(int x, int y, int id)
{
    if (id) {
        char buf[32] = { 0 };
        sprintf(buf, "%d", id);
        DrawText(buf, x, y - 30, 30, GREEN);
    }
    DrawRectangle(x, y, PLAYER_WIDTH, PLAYER_HEIGHT, RED);
}

void
DrawProjectile(int x, int y)
{
    int size = 100;

    Vector2 v1 = { x + 0, y + size };
    Vector2 v2 = { x + size, y + size * 2 };
    Vector2 v3 = { x + size, y + 0 };

    DrawTriangle(v1, v2, v3, BEIGE);
}

int
init_server_conn()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(SERVER_PORT);
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {

        printf("failed to connect to socket at %s: %s\n",
               "path TODO",
               strerror(errno));
        close(sockfd);
    }
    return sockfd;
fail:
    return -1;
}

int
read_possible_from_server(int fd)
{

    int r = 0;

    do {
        struct pollfd* pfd =
          (struct pollfd[]){ { .fd = fd, .events = POLLIN } };
        r = poll(pfd, 1, 0);
        if (r > 0 && pfd[0].revents & POLLIN) {
            uint32_t msg_head[2] = { 0 };
            ssize_t  n           = recv(fd, msg_head, sizeof(uint32_t) * 2, 0);
            if (n <= 0) {
                return 1;
            }

            if (msg_head[0] == MSG_DATA) {
                int32_t buf[msg_head[1]];
                memset(buf, 0, msg_head[1]);
                n = recv(fd, buf, msg_head[1], 0);
                if (n <= 0) {
                    return 1;
                }

                int pid = -1;

                for (int i = 0; buf[i] != -1; i++) {
                    if (buf[i] == MSG_PLAYER_ID) {
                        pid = buf[i + 1];

                        int new_player = 1;
                        dll_for_each(players, v)
                        {
                            if (pid != v->val.id)
                                continue;
                            new_player = 0;
                            break;
                        }

                        if (new_player) {
                            struct player p =
                              (struct player){ .id = pid, .x = 0, .y = 0 };
                            dll_push_tail(players, p)
                        }

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
            }
        }
    } while (r > 0);

    return 0;
}

int
main()
{
    int server_fd = init_server_conn();
    if (server_fd == -1) {
        printf("failed to connect to server\n");
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(1000);

    InitWindow(800, 600, "torta");
    // MaximizeWindow();

    char id[32]  = { 0 };
    int  n       = read(server_fd, id, sizeof(id));
    int  _int_id = atoi(id);
    printf("player id: %s\n", id);

    int shouldOpenInventar = 0;
    int pX                 = 0;
    int pY                 = 0;

    int isProjSpawned = 0;
    int projX         = 0;
    int projY         = 0;
    players           = (typeof(players))dll_init();

    int width  = GetRenderWidth();
    int height = GetRenderHeight();
    printf("w: %d, h: %d\n", width, height);
    while (!WindowShouldClose()) {
        if (IsWindowResized()) {
            width  = GetRenderWidth();
            height = GetRenderHeight();
            printf("w: %d, h: %d\n", width, height);
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);

        int _ipx = pX;
        int _ipy = pY;

        // update
        {
            if (read_possible_from_server(server_fd)) {
                printf("closed server conn");
                break;
            }

            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_K)) {
                pY = pY - PLAYER_SPEED;
            }
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_J)) {
                pY += PLAYER_SPEED;
            }
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_H)) {
                pX = pX - PLAYER_SPEED;
            }
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_L)) {
                pX += PLAYER_SPEED;
            }
            if (pX < 0) {
                pX = 0;
            }
            if (pX + PLAYER_WIDTH > width) {
                pX = width - PLAYER_WIDTH;
            }

            if (pY < 0) {
                pY = 0;
            }
            if (pY + PLAYER_HEIGHT > height) {
                pY = height - PLAYER_HEIGHT;
            }

            if (IsKeyPressed(KEY_I)) {
                printf("mydrilla\n");
                shouldOpenInventar = !shouldOpenInventar;
            }

            if (IsKeyPressed(KEY_S)) {
                send_msg_s(server_fd, "s pressed");
            }

            if (IsKeyDown(KEY_SPACE)) {
                isProjSpawned = 1;
            }

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                int mx = GetMouseX() * 2;
                int my = GetMouseY() * 2;
                pX     = mx;
                pY     = my;
            }

            if (isProjSpawned) {
                if (projX == 0 && projY == 0) {
                    projX = pX;
                    projY = pY;
                } else {
                    projX += PROJ_SPEED;

                    if (projX + PROJ_SIZE > width) {
                        isProjSpawned = 0;
                        projX         = 0;
                        projY         = 0;
                    }
                }
            }

            // player moved, send to server
            if (_ipx != pX || _ipy != pY) {
                send_msg_d(
                  server_fd,
                  (int32_t[]){
                    MSG_PLAYER_ID, _int_id, MSG_PLAYER_POS, pX, pY, -1 },
                  6);
            }
        }

        DrawPlayer(pX, pY, 0);

        {
            dll_for_each(players, v)
            {
                DrawPlayer(v->val.x, v->val.y, v->val.id);
            }
        }

        if (isProjSpawned)
            DrawProjectile(projX, projY);

        DrawText("My Drilla", 250, 200, 30, BLACK);

        if (shouldOpenInventar == 1) {
            DrawRectangle(100, 100, 200, 400, GOLD);
            DrawText("Inventar", 100, 100, 50, BLUE);
        }

        {
            int  fps = GetFPS();
            char fpsStr[30];
            snprintf(fpsStr, sizeof(fpsStr), "FPS: %d", fps);
            DrawText(fpsStr, 0, 0, 30, GREEN);
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
