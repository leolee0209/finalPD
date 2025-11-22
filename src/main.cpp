#include "raylib.h"
#include "constant.hpp"
#include "me.hpp"
#include "scene.hpp"
#include "attackManager.hpp"

int main(void)
{
    Vector2 sensitivity = {0.001f, 0.001f};
    // Initialization-----------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - 3d camera fps");

    Me player;
    Scene scene;

    DisableCursor();  // Limit cursor to relative movement inside the window
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Input---------------------------------------------------------------------------
        Vector2 mouseDelta = GetMouseDelta();
        player.getLookRotation().x -= mouseDelta.x * sensitivity.x;
        player.getLookRotation().y += mouseDelta.y * sensitivity.y;

        char sideway = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
        char forward = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
        bool crouching = IsKeyDown(KEY_LEFT_CONTROL);
        if (IsKeyPressed(KEY_B))
        {
            scene.am.getThousandAttack(&player)->spawnProjectile();
        }
        if (IsKeyPressed(KEY_E))
        {
            scene.am.getTripletAttack(&player)->spawnProjectile();
        }

        //----------------------------------------------------------------------------------

        // Update Player---------------------------------------------------------------------------
        player.UpdateBody(scene, sideway, forward, IsKeyPressed(KEY_SPACE), crouching);
        player.UpdateCamera(sideway, forward, crouching);
        //----------------------------------------------------------------------------------

        scene.Update();

        // Draw-----------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(player.getCamera());
        scene.DrawScene();
        EndMode3D();

        // Draw info box
        DrawRectangle(5, 5, 330, 75, Fade(SKYBLUE, 0.5f));
        DrawRectangleLines(5, 5, 330, 75, BLUE);

        DrawText("Camera controls:", 15, 15, 10, BLACK);
        DrawText("- Move keys: W, A, S, D, Space, Left-Ctrl", 15, 30, 10, BLACK);
        DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);
        // DrawText(TextFormat("- Velocity Len: (%06.3f)", Vector2Length({player.velocity.x, player.velocity.z})), 15, 60, 10, BLACK);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
