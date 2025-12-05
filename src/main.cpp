#include "raylib.h"
#include "constant.hpp"
#include "me.hpp"
#include "scene.hpp"
#include "attackManager.hpp"
#include "attack.hpp"
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
    
    // Load shared resources for enemies
    VanguardEnemy::LoadSharedResources();

    Me player;
    Scene scene;
    UIManager uiManager("mahjong.png", 9, 44, 60);
    uiManager.addElement(new UICrosshair({SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}));
    uiManager.addElement(new UIHealthBar(&player));
    uiManager.addElement(new UISelectedTileDisplay(&uiManager.muim, &player.hand));
    
    // Set player spawn position
    player.setSpawnPosition({0.0f, 0.0f, 0.0f});

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

        // Process tweak hotkeys even when the game is paused so tweaking is responsive
        DragonClawAttack *claw = scene.am.getDragonClawAttack(&player);
        if (claw)
        {
            claw->handleTweakHotkeys();
            // Tweak mode no longer pauses - allows testing attacks in real-time

            // Always refresh debug arc for tweak mode from main (camera/player state)
            if (DragonClawAttack::isTweakModeEnabled())
            {
                Camera cam = player.getCamera();
                Vector3 forward = Vector3Subtract(cam.target, cam.position);
                if (Vector3LengthSqr(forward) < 0.0001f) forward = {0.0f, 0.0f, -1.0f};
                forward = Vector3Normalize(forward);
                Vector3 right = Vector3Normalize(Vector3CrossProduct({0.0f,1.0f,0.0f}, forward));
                claw->refreshDebugArc(forward, right, player.pos());
            }
        }

        // Handle SeismicSlam tweak mode
        SeismicSlamAttack *slam = scene.am.getSeismicSlamAttack(&player);
        if (slam)
        {
            slam->handleTweakHotkeys();

            if (SeismicSlamAttack::isTweakModeEnabled())
            {
                // Use a local camera copy for debug arc computation without mutating player camera
                Camera cam = player.getCamera();
                slam->applyTweakCamera(player, cam);

                Vector3 forward = Vector3Subtract(cam.target, cam.position);
                if (Vector3LengthSqr(forward) < 0.0001f) forward = {0.0f, 0.0f, -1.0f};
                forward = Vector3Normalize(forward);
                forward.y = 0.0f; // Keep horizontal for SeismicSlam arc preview
                forward = Vector3Normalize(forward);
                Vector3 right = Vector3Normalize(Vector3CrossProduct({0.0f,1.0f,0.0f}, forward));
                slam->refreshDebugArc(forward, right, player.pos());
            }
        }

        // Always initialize frameInput to prevent input sticking
        char sideway = 0;
        char forward = 0;
        bool jumpPressed = false;
        bool crouching = false;
        
        if (!gamePaused)
        {
            Vector2 mouseDelta = GetMouseDelta();
            player.getLookRotation().x -= mouseDelta.x * sensitivity.x;
            player.getLookRotation().y += mouseDelta.y * sensitivity.y;

            sideway = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
            forward = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
            jumpPressed = IsKeyPressed(KEY_SPACE);
            crouching = IsKeyDown(KEY_LEFT_CONTROL);
        }
        else
        {
            // Consume the delta so it does not accumulate while the cursor is visible or tweak mode is active.
            GetMouseDelta();
        }
        
        PlayerInput frameInput(sideway, forward, jumpPressed, crouching);

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
                            bool canOpen = false;
                            // If door knows both connected rooms and both are cleared, open permanently
                            if (door->GetRoomA() && door->GetRoomB())
                            {
                                if (door->GetRoomA()->IsCompleted() && door->GetRoomB()->IsCompleted())
                                {
                                    canOpen = true;
                                }
                                else
                                {
                                    // Allow opening from the current room to enter the adjacent room
                                    // (player-initiated entry). The door will be closed by Scene::UpdateRoomDoors
                                    // after the player transitions unless both rooms become cleared.
                                    if (currentRoom == door->GetRoomA() || currentRoom == door->GetRoomB())
                                        canOpen = true;
                                }
                            }
                            else
                            {
                                // Fallback if room connections not set: allow if current room is completed
                                canOpen = currentRoom->IsCompleted();
                            }
                            if (canOpen)
                            {
                                door->Open();
                            }
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
                // Get the selected tile type to determine attack mode
                TileType selectedTile = uiManager.muim.getSelectedTile(player.hand);
                
                // Check tile type and use appropriate attack
                if (selectedTile >= TileType::CHARACTER_1 && selectedTile <= TileType::CHARACTER_9)
                {
                    // Character tile - Melee mode (Dragon's Claw)
                    DragonClawAttack *clawAttack = scene.am.getDragonClawAttack(&player);
                    if (clawAttack && clawAttack->canAttack())
                    {
                        clawAttack->spawnSlash(uc);
                    }
                }
                else if (selectedTile >= TileType::DOT_1 && selectedTile <= TileType::DOT_9)
                {
                    // Dot tile - Mage mode (Arcane Orb)
                    ArcaneOrbAttack *orbAttack = scene.am.getArcaneOrbAttack(&player);
                    if (orbAttack && orbAttack->canShoot())
                    {
                        orbAttack->spawnOrb(uc);
                    }
                }
                else if (selectedTile == TileType::DRAGON_WHITE)
                {
                    // White Dragon -> Mage mode (Arcane Orb)
                    ArcaneOrbAttack *orbAttack = scene.am.getArcaneOrbAttack(&player);
                    if (orbAttack && orbAttack->canShoot())
                    {
                        orbAttack->spawnOrb(uc);
                    }
                }
                else if (selectedTile == TileType::DRAGON_RED ||
                         (selectedTile >= TileType::WIND_EAST && selectedTile <= TileType::WIND_NORTH))
                {
                    // Red Dragon or Winds -> Melee mode (Dragon's Claw)
                    DragonClawAttack *clawAttack = scene.am.getDragonClawAttack(&player);
                    if (clawAttack && clawAttack->canAttack())
                    {
                        clawAttack->spawnSlash(uc);
                    }
                }
                else if (selectedTile == TileType::DRAGON_GREEN)
                {
                    // Green Dragon -> Ranger mode (Rapid Throw)
                    BasicTileAttack *basicAttack = scene.am.getBasicTileAttack(&player);
                    if (basicAttack)
                    {
                        basicAttack->spawnProjectile(uc);
                    }
                }
                else
                {
                    // Bamboo tiles and others -> Ranger mode (Rapid Throw)
                    BasicTileAttack *basicAttack = scene.am.getBasicTileAttack(&player);
                    if (basicAttack)
                    {
                        basicAttack->spawnProjectile(uc);
                    }
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
        if (DragonClawAttack::isTweakModeEnabled())
        {
            DragonClawAttack::applyTweakCamera(player, camera);
        }
        else if (SeismicSlamAttack::isTweakModeEnabled())
        {
            if (SeismicSlamAttack *slamCam = scene.am.getSeismicSlamAttack(&player))
            {
                slamCam->applyTweakCamera(player, camera);
            }
        }
        scene.SetViewPosition(camera.position);
        BeginMode3D(camera);
        scene.DrawScene(camera);
        EndMode3D();

        scene.DrawEnemyHealthDialogs(camera);
        scene.DrawDamageIndicators(camera);
        scene.DrawInteractionPrompts(player.pos(), camera);

        uiManager.draw(uc, player.hand);
        DragonClawAttack::drawTweakHud(player);
        if (SeismicSlamAttack::isTweakModeEnabled())
        {
            if (SeismicSlamAttack *slamHud = scene.am.getSeismicSlamAttack(&player))
            {
                slamHud->drawTweakHud(player);
            }
        }
        
        // Draw damage flash (red screen edge vignette)
        if (player.getDamageFlashAlpha() > 0.0f)
        {
            float alpha = player.getDamageFlashAlpha();
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            Color flashColor = ColorAlpha(RED, (unsigned char)(alpha * 180));
            
            // Draw vignette effect from edges
            int vignette = (int)(screenW * 0.15f);
            DrawRectangleGradientH(0, 0, vignette, screenH, flashColor, ColorAlpha(RED, 0));  // Left edge
            DrawRectangleGradientH(screenW - vignette, 0, vignette, screenH, ColorAlpha(RED, 0), flashColor);  // Right edge
            DrawRectangleGradientV(0, 0, screenW, vignette, flashColor, ColorAlpha(RED, 0));  // Top edge
            DrawRectangleGradientV(0, screenH - vignette, screenW, vignette, ColorAlpha(RED, 0), flashColor);  // Bottom edge
        }
        
        // Draw damage number next to health bar
        if (player.hasDamageNumber())
        {
            int screenW = GetScreenWidth();
            float fadeAlpha = 1.0f - player.getDamageNumberAlpha();  // Fade out over time
            float yOffset = player.getDamageNumberY();
            
            // Position near health bar (top left area)
            int baseX = screenW - 220;
            int baseY = 80 + (int)yOffset;
            
            const char *damageText = TextFormat("-%d", player.getLastDamageAmount());
            int fontSize = 32;
            Color textColor = ColorAlpha(RED, (unsigned char)(fadeAlpha * 255));
            Color outlineColor = ColorAlpha(DARKGRAY, (unsigned char)(fadeAlpha * 200));
            
            // Draw with outline for visibility
            DrawText(damageText, baseX - 1, baseY - 1, fontSize, outlineColor);
            DrawText(damageText, baseX + 1, baseY - 1, fontSize, outlineColor);
            DrawText(damageText, baseX - 1, baseY + 1, fontSize, outlineColor);
            DrawText(damageText, baseX + 1, baseY + 1, fontSize, outlineColor);
            DrawText(damageText, baseX, baseY, fontSize, textColor);
        }
        
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Ensure enemies are destroyed while window/context is alive
    scene.em.clear();
    uiManager.cleanup();
    
    // Cleanup shared resources
    VanguardEnemy::UnloadSharedResources();
    
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
