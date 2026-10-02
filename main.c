#include <raylib.h>
#include <stdio.h>

#define PLAYER_WIDTH  50
#define PLAYER_HEIGHT 50
#define PLAYER_SPEED  5
#define PROJ_SIZE     100
#define PROJ_SPEED    5

void
DrawPlayer(int x, int y)
{
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
main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    SetTargetFPS(60);

    InitWindow(800, 600, "torta");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    // MaximizeWindow();

    int shouldOpenInventar = 0;
    int pX                 = 0;
    int pY                 = 0;

    int isProjSpawned = 0;
    int projX         = 0;
    int projY         = 0;

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

        if (IsKeyDown(KEY_SPACE)) {
            isProjSpawned = 1;
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

        DrawPlayer(pX, pY);

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
