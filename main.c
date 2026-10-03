#include "dll.h"
#include "game.h"
#include <errno.h>
#include <raylib.h>

#include <curl/curl.h>
#include <unistd.h>
#ifdef _WIN32
#include <winsock2.h>
#define POLL(p, n, t) WSAPoll(p, n, t)
#else
#include <poll.h>
#include <sys/select.h>
#define POLL(p, n, t) poll(p, n, t)
#endif

#define SERVER_PORT "6767"
#define SERVER_ADDR "gamit.crol.bar"

#define PLAYER_WIDTH  50
#define PLAYER_HEIGHT 50
#define PLAYER_SPEED  5
#define PROJ_SIZE     100
#define PROJ_SPEED    5

#define PLAYER_POS_UPDATE_DELAY_MS 50

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

uint64_t
time_get_now_usec()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

CURL*
init_server_conn()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, "wss://gamit.crol.bar");
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        fprintf(stderr, "connect failed: %s\n", curl_easy_strerror(rc));
        goto fail;
    }

    printf("Connected to %s:%s\n", SERVER_ADDR, SERVER_PORT);

    return curl;
fail:
    return NULL;
}

int
send_ws_msg_d(CURL* curl, int32_t* data, size_t data_len)
{
    size_t body_size = sizeof(int32_t) * data_len;

    size_t   sent;
    CURLcode rc = curl_ws_send(curl,
                               (uint32_t[]){ MSG_DATA, body_size },
                               sizeof(uint32_t) * 2,
                               &sent,
                               0,
                               CURLWS_BINARY);
    if (rc != CURLE_OK) {
        printf("send: %s\n", curl_easy_strerror(rc));
        return 1;
    }

    rc = curl_ws_send(curl, data, body_size, &sent, 0, CURLWS_BINARY);
    if (rc != CURLE_OK) {
        printf("send: %s\n", curl_easy_strerror(rc));
        return 1;
    }

    return 0;
}

static ssize_t
ws_read(CURL* c, void* buf, size_t n, int block)
{
    if (n == 0)
        return 0;

    for (;;) {
        size_t                      got = 0;
        const struct curl_ws_frame* m   = NULL;
        CURLcode                    rc  = curl_ws_recv(c, buf, n, &got, &m);

        if (rc == CURLE_AGAIN) {
            if (!block) {
                errno = EAGAIN;
                return -1;
            }

            curl_socket_t s;
            if (curl_easy_getinfo(c, CURLINFO_ACTIVESOCKET, &s) != CURLE_OK ||
                s == CURL_SOCKET_BAD) {
                errno = EBADF;
                return -1;
            }

            struct pollfd p = { .fd = s, .events = POLLIN };
            int           pr;
            do {
                pr = poll(&p, 1, -1); /* sleep until readable */
            } while (pr < 0 && errno == EINTR);

            if (pr < 0)
                return -1;
            continue;
        }

        if (rc == CURLE_GOT_NOTHING) /* peer closed the connection */
            return 0;
        if (rc != CURLE_OK || m == NULL) {
            errno = EIO;
            return -1;
        }
        if (m->flags & CURLWS_CLOSE)
            return 0;
        if (m->flags & (CURLWS_PING | CURLWS_PONG))
            continue; /* control frame, not user data */
        if (got == 0)
            continue; /* empty data frame */

        return (ssize_t)got;
    }
}

static ssize_t
ws_recv(CURL* c, void* buf, size_t n)
{
    size_t                      got = 0;
    const struct curl_ws_frame* m;
    CURLcode                    rc = curl_ws_recv(c, buf, n, &got, &m);

    if (rc == CURLE_AGAIN)
        return 0;
    if (rc != CURLE_OK || (m->flags & CURLWS_CLOSE))
        return -1;
    if (m->flags & (CURLWS_PING | CURLWS_PONG))
        return 0;
    return (ssize_t)got;
}

int
read_possible_from_server(CURL* curl)
{
    int r = 0;
    do {
        uint32_t msg_head[2] = { 0 };
        r                    = ws_recv(curl, msg_head, sizeof(uint32_t) * 2);
        if (r < 0)
            return 1;
        if (r == 0)
            break;

        if (msg_head[0] == MSG_DATA) {
            int32_t buf[msg_head[1]];
            memset(buf, 0, msg_head[1]);
            if (ws_read(curl, buf, msg_head[1], 1) < 0) {
                return 1;
            }
            // for (int i = 0; buf[i] != -1; i++) {
            //     printf("r: %d ", buf[i]);
            // }
            // printf("\n");

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
    } while (r > 0);

    return 0;
}

int
send_pos_update(CURL* curl, int id, int pX, int pY)
{
    send_ws_msg_d(
      curl, (int32_t[]){ MSG_PLAYER_ID, id, MSG_PLAYER_POS, pX, pY, -1 }, 6);
    return 0;
}

int
main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(1000);

    InitWindow(800, 600, "torta");
    // MaximizeWindow();

    CURL* curl = init_server_conn();
    if (!curl) {
        printf("failed to connect to server\n");
        return 1;
    }

    int32_t id_buf[1] = { 0 };
    int     n         = ws_read(curl, id_buf, sizeof(int32_t), 1);
    char    id[32]    = { 0 };
    int     _int_id   = id_buf[0];
    sprintf(id, "%d", _int_id);
    printf("player id: %s\n", id);

    int shouldOpenInventar = 0;
    int pX                 = 0;
    int pY                 = 0;

    int isProjSpawned      = 0;
    int projX              = 0;
    int projY              = 0;
    players                = (typeof(players))dll_init();
    uint64_t lastPosUpdate = time_get_now_usec();
    int      posHasChanged = 0;

    int width  = GetRenderWidth();
    int height = GetRenderHeight();
    printf("w: %d, h: %d\n", width, height);
    while (!WindowShouldClose()) {
        int _ipx = pX;
        int _ipy = pY;

        // update
        {
            uint64_t n = time_get_now_usec();
            // every 800 ms send up
            if (n - lastPosUpdate > PLAYER_POS_UPDATE_DELAY_MS * 1000 &&
                posHasChanged) {
                send_pos_update(curl, _int_id, pX, pY);
                lastPosUpdate = n;
                posHasChanged = 0;
            }

            if (read_possible_from_server(curl)) {
                printf("closed server conn\n");
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
                // send_msg_s(curl, "s pressed");
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
                uint64_t n = time_get_now_usec();
                if (n - lastPosUpdate > PLAYER_POS_UPDATE_DELAY_MS * 1000) {
                    send_pos_update(curl, _int_id, pX, pY);
                    lastPosUpdate = n;
                    posHasChanged = 0;
                }
                posHasChanged = 1;
            }
        }

        if (IsWindowResized()) {
            width  = GetRenderWidth();
            height = GetRenderHeight();
            printf("w: %d, h: %d\n", width, height);
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);

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
        // usleep(1 * 1000);
    }

    CloseWindow();

    return 0;
}
