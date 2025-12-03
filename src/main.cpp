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
    SetExitKey(KEY_NULL);
    SearchAndSetResourceDir("resources");

    Me player;
    Scene scene;
    UIManager uiManager("mahjong.png", 9, 44, 60);
    uiManager.addElement(new UICrosshair({SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}));
    uiManager.addElement(new UIHealthBar(&player));
    
    // Set player spawn position
    player.setSpawnPosition({0.0f, 0.0f, 0.0f});
    
    // Assign tile textures to enemies after UIManager is ready
    scene.AssignEnemyTextures(&uiManager);

    // { // Create a charging enemy (mahjong tile)
    //     Enemy *enemy = new ChargingEnemy;
    //     enemy->obj().size = Vector3Scale({44, 60, 30}, 0.06); // Example size for a mahjong tile
    //     enemy->obj().pos = {0.0f, 1.0f, 0.0f};                // Example starting position
    //     enemy->setPosition(enemy->obj().pos);

    //     // Get the mahjong texture from the UIManager
    //     Texture2D &mahjongTexture = uiManager.muim.getSpriteSheet();
    //     enemy->obj().texture = &mahjongTexture;
    //     enemy->obj().useTexture = true;
    //     enemy->obj().sourceRect = uiManager.muim.getTile(TileType::BAMBOO_1); // Use entire texture

    //     scene.em.addEnemy(enemy); // Add the enemy to the scene
    // }

    // { // Create a shooter enemy with single bullet (mahjong tile)
    //     ShooterEnemy *enemy = new ShooterEnemy;
    //     enemy->obj().size = Vector3Scale({44, 60, 30}, 0.06f);
    //     enemy->obj().pos = {25.0f, 1.0f, -15.0f};
    //     enemy->setPosition(enemy->obj().pos);

    //     Texture2D &mahjongTexture = uiManager.muim.getSpriteSheet();
    //     enemy->obj().texture = &mahjongTexture;
    //     enemy->obj().useTexture = true;
    //     enemy->obj().sourceRect = uiManager.muim.getTile(TileType::BAMBOO_2);

    //     // Single bullet pattern (default)
    //     enemy->setBulletPattern(1, 0.0f);

    //     scene.em.addEnemy(enemy);
    // }

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
        {SlotBinding::Type::Key, KEY_R},
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
            // Handle interaction with briefcases and doors (C key)
            if (IsKeyPressed(KEY_C))
            {
                Vector3 playerPos = player.pos();
                // Check for door interaction
                Room *currentRoom = scene.GetCurrentPlayerRoom();
                if (currentRoom && currentRoom->IsCompleted())
                {
                    for (Door *door : currentRoom->GetDoors())
                    {
                        if (door && door->IsClosed() && door->IsPlayerNearby(playerPos, 5.0f))
                        {
                            // Check if both connected rooms are completed
                            // For now, just open the door
                            door->Open();
                            break;
                        }
                    }
                }
            }
            
            // Update room doors and player room tracking
            scene.UpdateRoomDoors(player.pos());
            
            // Handle basic attack (left click)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                BasicTileAttack *basicAttack = scene.am.getBasicTileAttack(&player);
                if (basicAttack)
                {
                    basicAttack->spawnProjectile(uc);
                }
            }

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

        // Briefcase menu update: UIManager queries Scene for activation/state
        // Must be outside gamePaused check so it can process clicks while menu is open
        uiManager.updateBriefcaseMenu(uc, player.hand, gamePaused);

        // Check for player death
        if (player.getHealth() <= 0 && !uiManager.isGameOverVisible())
        {
            uiManager.setGameOverVisible(true);
            EnableCursor(); // Show cursor for respawn button
        }

        // Handle respawn request
        if (uiManager.consumeRespawnRequest())
        {
            player.respawn(player.getSpawnPosition());
            uiManager.setGameOverVisible(false);
            gamePaused = false;
            uiManager.setPauseMenuVisible(false);
            DisableCursor();
        }

        uiManager.update(player.hand);

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

        ClearBackground(scene.getSkyColor());

        Camera camera = player.getCamera();
        scene.SetViewPosition(camera.position);
        BeginMode3D(camera);
        scene.DrawScene(camera);
        EndMode3D();

        scene.DrawEnemyHealthDialogs(camera);
        scene.DrawDamageIndicators(camera);
        scene.DrawInteractionPrompts(player.pos(), camera);

        uiManager.draw(uc, player.hand);
        
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Ensure enemies are destroyed while window/context is alive
    scene.em.clear();
    uiManager.cleanup();
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
