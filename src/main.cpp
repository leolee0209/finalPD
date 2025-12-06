#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include "constant.hpp"
#include "me.hpp"
#include "scene.hpp"
#include "attackManager.hpp"
#include "attack.hpp"
#include "uiManager.hpp"
#include "resource_dir.hpp"
#include "updateContext.hpp"
#include "openingScene.hpp"

enum class GameState
{
    MENU,
    TRANSITION,
    LOADING,
    GAMEPLAY,
    GAMEOVER
};

int main(void)
{
    Vector2 sensitivity = {0.001f, 0.001f};
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "mahjong");
    SetExitKey(KEY_NULL);
    SearchAndSetResourceDir("resources");
    InitAudioDevice();

    OpeningConfig openingCfg{}; // tune opening scene here
    OpeningScene opening(openingCfg);
    opening.Init();

    DisableCursor();
    SetTargetFPS(60);

    RenderTexture2D sceneTarget = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (sceneTarget.id != 0) SetTextureFilter(sceneTarget.texture, TEXTURE_FILTER_BILINEAR);

    Shader blurShader = LoadShader(0, "shaders/blur.fs");
    int blurStrengthLoc = (blurShader.id != 0) ? GetShaderLocation(blurShader, "blurStrength") : -1;
    int blurResolutionLoc = (blurShader.id != 0) ? GetShaderLocation(blurShader, "resolution") : -1;
    int blurCenterLoc = (blurShader.id != 0) ? GetShaderLocation(blurShader, "blurCenter") : -1;

    bool gamePaused = false;
    GameState gameState = GameState::MENU;
    float transitionTimer = 0.0f;
    float loadingTimer = 0.0f;
    const float loadingMinSeconds = 0.6f;
    bool assetsLoaded = false;
    bool loadingStarted = false;
    bool tweakMode = false;

    std::unique_ptr<Me> player;
    std::unique_ptr<Scene> scene;
    std::unique_ptr<UIManager> uiManager;

    Camera menuCamera{};
    // Use getters from opening instance because Init() might have loaded new config values
    menuCamera.position = {opening.GetCameraXStart(), opening.GetCameraYStart(), opening.GetCameraDistanceZ()};
    
    // In gameplay system, positive lookRotation.y means looking down (because pitch angle = -lookRotation.y)
    // So cameraPitchStart=-25° needs lookRotation.y=+25° (in radians)
    // Initialize Yaw (x) to PI to face towards +Z (where the table is), since targetOffset is -Z
    Vector2 menuLookRotation = {PI, -opening.GetCameraPitchStart() * DEG2RAD};
    
    // Calculate initial target using same system as gameplay camera (mycamera.cpp)
    const Vector3 up = {0.0f, 1.0f, 0.0f};
    const Vector3 targetOffset = {0.0f, 0.0f, -1.0f};
    Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, menuLookRotation.x);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));
    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, -menuLookRotation.y);
    menuCamera.target = Vector3Add(menuCamera.position, pitch);
    menuCamera.up = {0.0f, 1.0f, 0.0f};
    menuCamera.fovy = opening.GetCameraFov();
    menuCamera.projection = CAMERA_PERSPECTIVE;

    struct SlotBinding
    {
        enum class Type { Mouse, Key } type;
        int code;
    };
    const SlotBinding slotBindings[3] = {
        {SlotBinding::Type::Mouse, MOUSE_BUTTON_RIGHT},
        {SlotBinding::Type::Key, KEY_R},
        {SlotBinding::Type::Key, KEY_E}
    };

    int frameCounter = 0;
    while (!WindowShouldClose())
    {
        frameCounter++;
        float dt = GetFrameTime();
        bool haveGame = (player && scene && uiManager);
        bool inGameplay = haveGame && (gameState == GameState::GAMEPLAY);

        auto ensureAssetsLoaded = [&]() {
            if (assetsLoaded) return;
            VanguardEnemy::LoadSharedResources();
            player = std::make_unique<Me>();
            scene = std::make_unique<Scene>();
            uiManager = std::make_unique<UIManager>("mahjong.png", 9, 44, 60);
            uiManager->addElement(new UICrosshair({SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f}));
            uiManager->addElement(new UIHealthBar(player.get()));
            uiManager->addElement(new UISelectedTileDisplay(&uiManager->muim, &player->hand));
            player->setSpawnPosition({0.0f, 0.0f, 0.0f});
            assetsLoaded = true;
        };

        if (inGameplay && IsKeyPressed(KEY_ESCAPE))
        {
            gamePaused = !gamePaused;
            uiManager->setPauseMenuVisible(gamePaused);
            if (gamePaused) EnableCursor(); else DisableCursor();
        }

        if (gameState == GameState::MENU)
        {
            if (!tweakMode)
            {
                // Mouse Look
                Vector2 mouseDelta = GetMouseDelta();
                if (frameCounter > 10) // Skip first few frames to avoid mouse jump
                {
                    menuLookRotation.x -= mouseDelta.x * sensitivity.x;
                    menuLookRotation.y += mouseDelta.y * sensitivity.y;
                }
                
                // Clamp pitch (looking up/down)
                menuLookRotation.y = Clamp(menuLookRotation.y, -89.0f * DEG2RAD, 89.0f * DEG2RAD);

                // Update Camera Target
                const Vector3 up = {0.0f, 1.0f, 0.0f};
                const Vector3 targetOffset = {0.0f, 0.0f, -1.0f};
                Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, menuLookRotation.x);
                Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));
                Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, -menuLookRotation.y);
                menuCamera.target = Vector3Add(menuCamera.position, pitch);
            }

            if (IsKeyPressed(KEY_ENTER))
            {
                gameState = GameState::TRANSITION;
                transitionTimer = 0.0f;
                loadingStarted = false;
            }
        }

        if (gameState == GameState::TRANSITION)
        {
            transitionTimer += dt;

            if (!loadingStarted && transitionTimer >= opening.GetImpactTime())
            {
                loadingStarted = true;
                loadingTimer = 0.0f;
                ensureAssetsLoaded();
            }
            else if (loadingStarted)
            {
                loadingTimer += dt;
            }

            if (transitionTimer >= opening.GetTotalDuration())
            {
                gameState = GameState::LOADING;
            }
        }

        if (gameState == GameState::LOADING)
        {
            if (!loadingStarted)
            {
                loadingStarted = true;
                loadingTimer = 0.0f;
                ensureAssetsLoaded();
            }
            loadingTimer += dt;
            if (loadingTimer >= loadingMinSeconds && assetsLoaded)
            {
                gameState = GameState::GAMEPLAY;
                gamePaused = false;
                uiManager->setPauseMenuVisible(false);
                DisableCursor();
            }
        }

        // Tweak hotkeys even when paused
        DragonClawAttack *claw = (haveGame ? scene->am.getDragonClawAttack(player.get()) : nullptr);
        if (claw)
        {
            claw->handleTweakHotkeys();
            if (DragonClawAttack::isTweakModeEnabled())
            {
                Camera cam = player->getCamera();
                Vector3 forward = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
                if (Vector3LengthSqr(forward) < 0.0001f) forward = {0.0f, 0.0f, -1.0f};
                Vector3 right = Vector3Normalize(Vector3CrossProduct({0.0f, 1.0f, 0.0f}, forward));
                claw->refreshDebugArc(forward, right, player->pos());
            }
        }

        SeismicSlamAttack *slam = (haveGame ? scene->am.getSeismicSlamAttack(player.get()) : nullptr);
        if (slam)
        {
            slam->handleTweakHotkeys();
            if (SeismicSlamAttack::isTweakModeEnabled())
            {
                Camera cam = player->getCamera();
                slam->applyTweakCamera(*player, cam);
                Vector3 forward = Vector3Subtract(cam.target, cam.position);
                if (Vector3LengthSqr(forward) < 0.0001f) forward = {0.0f, 0.0f, -1.0f};
                forward.y = 0.0f;
                forward = Vector3Normalize(forward);
                Vector3 right = Vector3Normalize(Vector3CrossProduct({0.0f, 1.0f, 0.0f}, forward));
                slam->refreshDebugArc(forward, right, player->pos());
            }
        }

        char sideway = 0;
        char forwardInput = 0;
        bool jumpPressed = false;
        bool crouching = false;
        if (inGameplay && !gamePaused)
        {
            Vector2 md = GetMouseDelta();
            player->getLookRotation().x -= md.x * sensitivity.x;
            player->getLookRotation().y += md.y * sensitivity.y;
            sideway = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
            forwardInput = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
            jumpPressed = IsKeyPressed(KEY_SPACE);
            crouching = IsKeyDown(KEY_LEFT_CONTROL);
        }
        else
        {
            GetMouseDelta();
        }

        PlayerInput frameInput(sideway, forwardInput, jumpPressed, crouching);
        UpdateContext uc(scene.get(), player.get(), frameInput, uiManager.get());

        if (inGameplay && !gamePaused)
        {
            if (IsKeyPressed(KEY_C))
            {
                Vector3 playerPos = player->pos();
                Room *currentRoom = scene->GetCurrentPlayerRoom();
                if (currentRoom && currentRoom->IsCompleted())
                {
                    for (Door *door : currentRoom->GetDoors())
                    {
                        if (door && door->IsClosed() && door->IsPlayerNearby(playerPos, 5.0f))
                        {
                            bool canOpen = false;
                            if (door->GetRoomA() && door->GetRoomB())
                            {
                                if (door->GetRoomA()->IsCompleted() && door->GetRoomB()->IsCompleted())
                                {
                                    canOpen = true;
                                }
                                else if (currentRoom == door->GetRoomA() || currentRoom == door->GetRoomB())
                                {
                                    canOpen = true;
                                }
                            }
                            else
                            {
                                canOpen = currentRoom->IsCompleted();
                            }
                            if (canOpen) door->Open();
                            break;
                        }
                    }
                }
            }

            scene->UpdateRoomDoors(player->pos());

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                TileType selectedTile = uiManager->muim.getSelectedTile(player->hand);
                if (selectedTile >= TileType::CHARACTER_1 && selectedTile <= TileType::CHARACTER_9)
                {
                    DragonClawAttack *clawAttack = scene->am.getDragonClawAttack(player.get());
                    if (clawAttack && clawAttack->canAttack()) clawAttack->spawnSlash(uc);
                }
                else if (selectedTile >= TileType::DOT_1 && selectedTile <= TileType::DOT_9)
                {
                    ArcaneOrbAttack *orbAttack = scene->am.getArcaneOrbAttack(player.get());
                    if (orbAttack && orbAttack->canShoot()) orbAttack->spawnOrb(uc);
                }
                else if (selectedTile == TileType::DRAGON_WHITE)
                {
                    ArcaneOrbAttack *orbAttack = scene->am.getArcaneOrbAttack(player.get());
                    if (orbAttack && orbAttack->canShoot()) orbAttack->spawnOrb(uc);
                }
                else if (selectedTile == TileType::DRAGON_RED || (selectedTile >= TileType::WIND_EAST && selectedTile <= TileType::WIND_NORTH))
                {
                    DragonClawAttack *clawAttack = scene->am.getDragonClawAttack(player.get());
                    if (clawAttack && clawAttack->canAttack()) clawAttack->spawnSlash(uc);
                }
                else if (selectedTile == TileType::DRAGON_GREEN)
                {
                    BambooBasicAttack *basicAttack = scene->am.getBasicTileAttack(player.get());
                    if (basicAttack) basicAttack->spawnProjectile(uc);
                }
                else
                {
                    BambooBasicAttack *basicAttack = scene->am.getBasicTileAttack(player.get());
                    if (basicAttack) basicAttack->spawnProjectile(uc);
                }
            }

            for (int slotIdx = 0; slotIdx < 3; ++slotIdx)
            {
                bool pressed = (slotBindings[slotIdx].type == SlotBinding::Type::Mouse)
                                   ? IsMouseButtonPressed(static_cast<MouseButton>(slotBindings[slotIdx].code))
                                   : IsKeyPressed(static_cast<KeyboardKey>(slotBindings[slotIdx].code));
                if (pressed)
                {
                    scene->am.triggerSlotAttack(slotIdx, uc);
                }
            }

            player->UpdateBody(uc);
            player->UpdateCamera(uc);
            scene->Update(uc);
        }

        if (haveGame)
        {
            uiManager->updateBriefcaseMenu(uc, player->hand, gamePaused);
        }

        if (inGameplay && player->getHealth() <= 0 && !uiManager->isGameOverVisible())
        {
            uiManager->setGameOverVisible(true);
            EnableCursor();
            gameState = GameState::GAMEOVER;
        }

        if (haveGame && uiManager->consumeRespawnRequest())
        {
            player->respawn(player->getSpawnPosition());
            uiManager->setGameOverVisible(false);
            gamePaused = false;
            uiManager->setPauseMenuVisible(false);
            DisableCursor();
            gameState = GameState::GAMEPLAY;
        }

        if (haveGame) uiManager->update(player->hand);

        if (haveGame && uiManager->consumeResumeRequest())
        {
            gamePaused = false;
            uiManager->setPauseMenuVisible(false);
            DisableCursor();
        }

        if (haveGame && uiManager->consumeQuitRequest())
        {
            break;
        }

        if (IsKeyPressed(KEY_F4)) tweakMode = !tweakMode;

        // Draw
        float radialBlur = 0.0f;
        float vignetteStrength = 0.35f;
        float blackoutAlpha = 0.0f;
        Camera camera = haveGame ? player->getCamera() : menuCamera;
        if (gameState == GameState::GAMEPLAY || gameState == GameState::GAMEOVER)
        {
            if (DragonClawAttack::isTweakModeEnabled()) DragonClawAttack::applyTweakCamera(*player, camera);
            else if (SeismicSlamAttack::isTweakModeEnabled())
            {
                if (SeismicSlamAttack *slamCam = scene->am.getSeismicSlamAttack(player.get()))
                {
                    slamCam->applyTweakCamera(*player, camera);
                }
            }
        }
        else
        {
            if (tweakMode)
            {
                opening.UpdateTweakMode(menuCamera);
                camera = menuCamera;
                if (IsKeyPressed(KEY_F5)) opening.SaveConfig();
            }
            else
            {
                TransitionVisuals tv = opening.EvaluateTransition(transitionTimer);
                if (tv.triggerImpactAudio) opening.PlayImpactAudio();

                if (gameState == GameState::MENU)
                {
                    // Update menuCamera position from config
                    menuCamera.position = {tv.camX, tv.camY, tv.camZ};
                    menuCamera.fovy = opening.GetCameraFov();

                    // Target is already updated by mouse look logic in the loop
                    // But we need to re-apply it relative to the potentially new position
                    const Vector3 up = {0.0f, 1.0f, 0.0f};
                    const Vector3 targetOffset = {0.0f, 0.0f, -1.0f};
                    Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, menuLookRotation.x);
                    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));
                    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, -menuLookRotation.y);
                    menuCamera.target = Vector3Add(menuCamera.position, pitch);
                    
                    camera = menuCamera;
                }
                else
                {
                    // Transition/Loading: Override camera with animation
                    camera.position = {tv.camX, tv.camY, tv.camZ};
                    camera.fovy = opening.GetCameraFov();
                    
                    float pitchRad = tv.pitchDeg * DEG2RAD;
                    Vector3 forwardVec = {0.0f, sinf(pitchRad), cosf(pitchRad)};
                    camera.target = Vector3Add(camera.position, forwardVec);
                    camera.up = {0.0f, 1.0f, 0.0f};
                    camera.projection = CAMERA_PERSPECTIVE;
                }
                
                radialBlur = tv.radialBlur;
                vignetteStrength = tv.vignetteStrength;
                blackoutAlpha = tv.blackoutAlpha;
            }
        }

        float damageBlur = (haveGame ? player->getDamageFlashAlpha() * 0.6f : 0.0f);
        float blurStrength = Clamp(std::max(radialBlur, damageBlur), 0.0f, 1.25f);
        bool useBlur = (sceneTarget.id != 0) && (blurShader.id != 0) && blurStrength > 0.001f;

        bool drewToTarget = false;
        if (sceneTarget.id != 0)
        {
            drewToTarget = true;
            BeginTextureMode(sceneTarget);
            if (gameState == GameState::MENU || gameState == GameState::TRANSITION || gameState == GameState::LOADING || !haveGame)
            {
                ClearBackground(BLACK);
                if (gameState != GameState::LOADING)
                {
                    opening.DrawMenuScene(camera, GetScreenWidth(), GetScreenHeight(), gameState == GameState::MENU);
                    opening.DrawSpotlightMask(GetScreenWidth(), GetScreenHeight());
                }
            }
            else
            {
                ClearBackground(scene->getSkyColor());
                scene->SetViewPosition(camera.position);
                BeginMode3D(camera);
                scene->DrawScene(camera);
                EndMode3D();
                scene->DrawEnemyHealthDialogs(camera);
                scene->DrawDamageIndicators(camera);
                scene->DrawInteractionPrompts(player->pos(), camera);
            }
            EndTextureMode();
        }

        BeginDrawing();
        ClearBackground(BLACK);

        if (drewToTarget)
        {
            Rectangle src{0.0f, 0.0f, (float)sceneTarget.texture.width, -(float)sceneTarget.texture.height};
            Rectangle dst{0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight()};
            if (useBlur)
            {
                Vector2 res{(float)GetScreenWidth(), (float)GetScreenHeight()};
                if (blurStrengthLoc >= 0) SetShaderValue(blurShader, blurStrengthLoc, &blurStrength, SHADER_UNIFORM_FLOAT);
                if (blurResolutionLoc >= 0) SetShaderValue(blurShader, blurResolutionLoc, &res.x, SHADER_UNIFORM_VEC2);
                Vector2 centerUv{0.5f, 0.5f};
                if (blurCenterLoc >= 0) SetShaderValue(blurShader, blurCenterLoc, &centerUv.x, SHADER_UNIFORM_VEC2);
                BeginShaderMode(blurShader);
                DrawTexturePro(sceneTarget.texture, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
                EndShaderMode();
            }
            else
            {
                DrawTexturePro(sceneTarget.texture, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
            }
        }
        else
        {
            ClearBackground(haveGame ? scene->getSkyColor() : BLACK);
            if (haveGame)
            {
                scene->SetViewPosition(camera.position);
                BeginMode3D(camera);
                scene->DrawScene(camera);
                EndMode3D();
                scene->DrawEnemyHealthDialogs(camera);
                scene->DrawDamageIndicators(camera);
                scene->DrawInteractionPrompts(player->pos(), camera);
            }
        }

        if (gameState == GameState::GAMEPLAY || gameState == GameState::GAMEOVER)
        {
            uiManager->draw(uc, player->hand);
            DragonClawAttack::drawTweakHud(*player);
            if (SeismicSlamAttack::isTweakModeEnabled())
            {
                if (SeismicSlamAttack *slamHud = scene->am.getSeismicSlamAttack(player.get()))
                {
                    slamHud->drawTweakHud(*player);
                }
            }
        }

        if (haveGame && player->getDamageFlashAlpha() > 0.0f)
        {
            float alpha = player->getDamageFlashAlpha();
            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            Color flashColor = ColorAlpha(RED, (unsigned char)(alpha * 180));
            int vignette = (int)(screenW * 0.15f);
            DrawRectangleGradientH(0, 0, vignette, screenH, flashColor, ColorAlpha(RED, 0));
            DrawRectangleGradientH(screenW - vignette, 0, vignette, screenH, ColorAlpha(RED, 0), flashColor);
            DrawRectangleGradientV(0, 0, screenW, vignette, flashColor, ColorAlpha(RED, 0));
            DrawRectangleGradientV(0, screenH - vignette, screenW, vignette, ColorAlpha(RED, 0), flashColor);
        }

        if (haveGame && player->hasDamageNumber())
        {
            int screenW = GetScreenWidth();
            float fadeAlpha = 1.0f - player->getDamageNumberAlpha();
            float yOffset = player->getDamageNumberY();
            int baseX = screenW - 220;
            int baseY = 80 + (int)yOffset;
            const char *damageText = TextFormat("-%d", player->getLastDamageAmount());
            int fontSize = 32;
            Color textColor = ColorAlpha(RED, (unsigned char)(fadeAlpha * 255));
            Color outlineColor = ColorAlpha(DARKGRAY, (unsigned char)(fadeAlpha * 200));
            DrawText(damageText, baseX - 1, baseY - 1, fontSize, outlineColor);
            DrawText(damageText, baseX + 1, baseY - 1, fontSize, outlineColor);
            DrawText(damageText, baseX - 1, baseY + 1, fontSize, outlineColor);
            DrawText(damageText, baseX + 1, baseY + 1, fontSize, outlineColor);
            DrawText(damageText, baseX, baseY, fontSize, textColor);
        }

        if (gameState == GameState::MENU || gameState == GameState::TRANSITION)
        {
            opening.DrawVignetteAndBlackout(GetScreenWidth(), GetScreenHeight(), vignetteStrength, blackoutAlpha);
            if (tweakMode) opening.DrawTweakUI();
        }

        if (gameState == GameState::MENU)
        {
        }
        else if (gameState == GameState::TRANSITION)
        {
            DrawText("Dozing off...", 32, SCREEN_HEIGHT - 64, 28, Fade(WHITE, 0.8f));
        }
        else if (gameState == GameState::LOADING)
        {
            const char *loadingText = assetsLoaded ? "Finalizing..." : "Loading assets...";
            int fontSize = 32;
            int textWidth = MeasureText(loadingText, fontSize);
            DrawText(loadingText, SCREEN_WIDTH / 2 - textWidth / 2, SCREEN_HEIGHT / 2 - fontSize / 2, fontSize, WHITE);
        }

        EndDrawing();
    }

    if (scene) scene->em.clear();
    if (uiManager) uiManager->cleanup();
    if (blurShader.id != 0) UnloadShader(blurShader);
    if (sceneTarget.id != 0) UnloadRenderTexture(sceneTarget);
    opening.Cleanup();
    VanguardEnemy::UnloadSharedResources();
    CloseWindow();
    CloseAudioDevice();
    return 0;
}
