#include "raylib.h"
#include "constant.hpp"
#include "me.hpp"
#include "scene.hpp"
#include "attackManager.hpp"
#include "uiManager.hpp"
#include "resource_dir.hpp"
#include "updateContext.hpp"
#include "uiHealthBar.hpp"
int main(void)
{
    Vector2 sensitivity = {0.001f, 0.001f};
    // Initialization-----------------------------------------------------------------------
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "mahjong");
    SetExitKey(KEY_NULL);
    SearchAndSetResourceDir("resources");

    Me player;
    Scene scene;
    UIManager uiManager("mahjong.png", 9, 44, 60);

    uiManager.muim.createPlayerHand(SCREEN_WIDTH, SCREEN_HEIGHT);
    uiManager.addElement(new UICrosshair({SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}));
    uiManager.addElement(new UIHealthBar(&player));

    { // Create a charging enemy (mahjong tile)
        Enemy *enemy = new ChargingEnemy;
        enemy->obj().size = Vector3Scale({44, 60, 30}, 0.06); // Example size for a mahjong tile
        enemy->obj().pos = {0.0f, 1.0f, 0.0f};                // Example starting position

        // Get the mahjong texture from the UIManager
        Texture2D &mahjongTexture = uiManager.muim.getSpriteSheet();
        enemy->obj().texture = &mahjongTexture;
        enemy->obj().useTexture = true;
        enemy->obj().sourceRect = uiManager.muim.getTile(MahjongTileType::BAMBOO_1); // Use entire texture

        scene.em.addEnemy(enemy); // Add the enemy to the scene
    }

    { // Create a shooter enemy (mahjong tile)
        Enemy *enemy = new ShooterEnemy;
        enemy->obj().size = Vector3Scale({44, 60, 30}, 0.06f);
        enemy->obj().pos = {25.0f, 1.0f, -15.0f};

        Texture2D &mahjongTexture = uiManager.muim.getSpriteSheet();
        enemy->obj().texture = &mahjongTexture;
        enemy->obj().useTexture = true;
        enemy->obj().sourceRect = uiManager.muim.getTile(MahjongTileType::BAMBOO_2);

        scene.em.addEnemy(enemy);
    }

    DisableCursor();  // Limit cursor to relative movement inside the window
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second

    bool gamePaused = false;
    struct SlotBinding
    {
        enum class Type
        {
            Mouse,
            Key
        } type;
        int code;
    };
    const SlotBinding slotBindings[3] = {
        {SlotBinding::Type::Mouse, MOUSE_BUTTON_RIGHT},
        {SlotBinding::Type::Mouse, MOUSE_BUTTON_LEFT},
        {SlotBinding::Type::Key, KEY_E}};

    // Main game loop
    while (true)
    {
        if (WindowShouldClose())
        {
            break;
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            gamePaused = !gamePaused;
            uiManager.setPauseMenuVisible(gamePaused);
            if (gamePaused)
            {
                EnableCursor();
            }
            else
            {
                DisableCursor();
            }
        }

        PlayerInput frameInput(0, 0, false, false);
        if (!gamePaused)
        {
            Vector2 mouseDelta = GetMouseDelta();
            player.getLookRotation().x -= mouseDelta.x * sensitivity.x;
            player.getLookRotation().y += mouseDelta.y * sensitivity.y;

            char sideway = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
            char forward = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
            bool crouching = IsKeyDown(KEY_LEFT_CONTROL);
            frameInput = PlayerInput(sideway, forward, IsKeyPressed(KEY_SPACE), crouching);
        }
        else
        {
            // Consume the delta so it does not accumulate while the cursor is visible.
            GetMouseDelta();
        }

        UpdateContext uc(&scene, &player, frameInput, &uiManager);

        if (!gamePaused)
        {
            for (int slotIdx = 0; slotIdx < 3; ++slotIdx)
            {
                bool pressed = false;
                if (slotBindings[slotIdx].type == SlotBinding::Type::Mouse)
                {
                    pressed = IsMouseButtonPressed(static_cast<MouseButton>(slotBindings[slotIdx].code));
                }
                else
                {
                    pressed = IsKeyPressed(static_cast<KeyboardKey>(slotBindings[slotIdx].code));
                }

                if (pressed)
                {
                    scene.am.triggerSlotAttack(slotIdx, uc);
                }
            }

            player.UpdateBody(uc);
            player.UpdateCamera(uc);
            scene.Update(uc); // Pass the player to the scene update
        }

        uiManager.update();

        if (uiManager.consumeResumeRequest())
        {
            gamePaused = false;
            uiManager.setPauseMenuVisible(false);
            DisableCursor();
        }

        if (uiManager.consumeQuitRequest())
        {
            break;
        }

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
