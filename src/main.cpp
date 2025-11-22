#include "raylib.h"
#include "constant.hpp"
#include "me.hpp"
#include "scene.hpp"
#include "attackManager.hpp"
#include "uiManager.hpp"
#include "resource_dir.hpp"

int main(void)
{
    Vector2 sensitivity = {0.001f, 0.001f};
    // Initialization-----------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - 3d camera fps");
    SearchAndSetResourceDir("resources");

    Me player;
    Scene scene;
    UIManager uiManager("mahjong.png", 9, 44, 60);
    uiManager.muim.addTile(MahjongTileType::CHARACTER_6, {10, 10});

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
        uiManager.update();

        // Draw-----------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(player.getCamera());
        scene.DrawScene();
        EndMode3D();

        uiManager.draw();

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    uiManager.cleanup();
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
