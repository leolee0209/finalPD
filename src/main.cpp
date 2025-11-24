#include "raylib.h"
#include "constant.hpp"
#include "me.hpp"
#include "scene.hpp"
#include "attackManager.hpp"
#include "uiManager.hpp"
#include "resource_dir.hpp"
#include "updateContext.hpp"
int main(void)
{
    Vector2 sensitivity = {0.001f, 0.001f};
    // Initialization-----------------------------------------------------------------------
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "mahjong");
    SearchAndSetResourceDir("resources");

    Me player;
    Scene scene;
    UIManager uiManager("mahjong.png", 9, 44, 60);
    uiManager.muim.createPlayerHand(SCREEN_WIDTH, SCREEN_HEIGHT);
    
    uiManager.addElement(new UICrosshair({SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}));

    { // Create an enemy (mahjong tile)
        Enemy *enemy = new Enemy;
        enemy->obj().size = Vector3Scale({44, 60, 40},0.06);  // Example size for a mahjong tile
        enemy->obj().pos = {0.0f, 1.0f, 0.0f}; // Example starting position

        // Get the mahjong texture from the UIManager
        Texture2D &mahjongTexture = uiManager.muim.getSpriteSheet();
        enemy->obj().texture = &mahjongTexture;
        enemy->obj().useTexture = true;
        enemy->obj().sourceRect = uiManager.muim.getTile(MahjongTileType::BAMBOO_1); // Use entire texture

        scene.em.addEnemy(enemy); // Add the enemy to the scene
    }

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
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            MahjongTileType selectedTile = uiManager.muim.getSelectedTile();
            if (selectedTile != MahjongTileType::EMPTY)
            {
                Texture2D &texture = uiManager.muim.getSpriteSheet();
                Rectangle rect = uiManager.muim.getTile(selectedTile);
                scene.am.recordThrow(selectedTile, &player, &texture, rect);
            }
        }

        UpdateContext uc(&scene, &player, PlayerInput(sideway, forward, IsKeyPressed(KEY_SPACE), crouching));

        //----------------------------------------------------------------------------------

        // Update Player---------------------------------------------------------------------------
        player.UpdateBody(uc);
        player.UpdateCamera(uc);
        //----------------------------------------------------------------------------------

        scene.Update(uc); // Pass the player to the scene update
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
