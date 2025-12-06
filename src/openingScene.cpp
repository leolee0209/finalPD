#include "openingScene.hpp"
#include "rlights.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>

OpeningScene::OpeningScene(const OpeningConfig &cfg) : config(cfg) {}

void OpeningScene::RebuildTable()
{
    // No longer generating mesh, using loaded model
}

bool OpeningScene::Init()
{
    LoadConfig();
    
    // Load Table Model
    if (FileExists("mahjong_table.glb"))
    {
        tableModel = LoadModel("mahjong_table.glb");
        BoundingBox bb = GetModelBoundingBox(tableModel);
        Vector3 size = Vector3Subtract(bb.max, bb.min);
        std::cout << "Loaded mahjong_table.glb. Size: " << size.x << ", " << size.y << ", " << size.z << std::endl;
        
        // Auto-scale if it looks huge (e.g. > 10 units)
        if (size.x > 10.0f && config.modelScale == 1.0f)
        {
            config.modelScale = 0.01f; // Assume cm -> m conversion needed
            std::cout << "Auto-scaling model by 0.01" << std::endl;
        }
    }
    else
    {
        // Fallback if model missing
        tableMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
        tableModel = LoadModelFromMesh(tableMesh);
    }
    
    // Setup lighting system
    if (config.enableRealisticLighting)
    {
        SetupLighting();
    }
    
    audioReady = IsAudioDeviceReady();
    if (audioReady)
    {
        thudSound = MakeTone(config.thudFreq, config.thudDuration, config.thudVolume, config.thudDecay);
        rumbleSound = MakeTone(config.rumbleFreq, config.rumbleDuration, config.rumbleVolume, config.rumbleDecay);
    }
    return true;
}

void OpeningScene::SetupLighting()
{
    // Load lighting shader
    if (FileExists("shaders/lighting.vs") && FileExists("shaders/lighting.fs"))
    {
        lightingShader = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
        
        if (lightingShader.id > 0)
        {
            // Get shader locations
            lightingShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightingShader, "viewPos");
            
            // Set ambient light
            int ambientLoc = GetShaderLocation(lightingShader, "ambient");
            float ambientValue[4] = {
                config.ambientColorR / 255.0f * config.ambientIntensity,
                config.ambientColorG / 255.0f * config.ambientIntensity,
                config.ambientColorB / 255.0f * config.ambientIntensity,
                1.0f
            };
            SetShaderValue(lightingShader, ambientLoc, ambientValue, SHADER_UNIFORM_VEC4);
            
            // Create main point light
            Vector3 lightPos = {config.lightPosX, config.lightPosY, config.lightPosZ};
            Color lightCol = {
                (unsigned char)Clamp(config.lightColorR, 0.0f, 255.0f),
                (unsigned char)Clamp(config.lightColorG, 0.0f, 255.0f),
                (unsigned char)Clamp(config.lightColorB, 0.0f, 255.0f),
                255
            };
            mainLight = CreateLight(LIGHT_POINT, lightPos, Vector3Zero(), lightCol, lightingShader);
            mainLight.attenuation = config.lightIntensity;
            
            // Apply shader to table model materials
            for (int i = 0; i < tableModel.materialCount; i++)
            {
                tableModel.materials[i].shader = lightingShader;
            }
            
            lightingSetup = true;
            std::cout << "Realistic lighting enabled (Warm: " << config.lightColorR << "," 
                      << config.lightColorG << "," << config.lightColorB << ")" << std::endl;
        }
        else
        {
            std::cerr << "Failed to load lighting shader" << std::endl;
        }
    }
    else
    {
        std::cerr << "Lighting shader files not found" << std::endl;
    }
}

void OpeningScene::Cleanup()
{
    if (tableModel.meshCount > 0) UnloadModel(tableModel);
    if (lightingShader.id > 0) UnloadShader(lightingShader);
    if (audioReady)
    {
        if (thudSound.frameCount > 0) UnloadSound(thudSound);
        if (rumbleSound.frameCount > 0) UnloadSound(rumbleSound);
    }
}

bool OpeningScene::RayPlaneIntersection(const Ray &ray, float planeY, Vector3 &hit) const
{
    float denom = ray.direction.y;
    if (fabsf(denom) < 0.0001f) return false;
    float t = (planeY - ray.position.y) / denom;
    if (t < 0.0f) return false;
    hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    return true;
}

Sound OpeningScene::MakeTone(float freq, float duration, float volume, float decay) const
{
    int sampleRate = 44100;
    int frameCount = (int)(duration * sampleRate);
    Wave wave{};
    wave.frameCount = frameCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    short *data = (short *)MemAlloc(frameCount * sizeof(short));
    const float twoPi = 6.28318530718f;
    for (int i = 0; i < frameCount; ++i)
    {
        float t = (float)i / (float)sampleRate;
        float env = expf(-decay * t);
        float sample = sinf(twoPi * freq * t) * env * volume;
        data[i] = (short)(sample * 32767.0f);
    }
    wave.data = data;
    Sound s = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return s;
}



TransitionVisuals OpeningScene::EvaluateTransition(float tSeconds)
{
    TransitionVisuals v{};
    v.pitchDeg = config.cameraPitchStart;
    v.camX = config.cameraXStart;
    v.camY = config.cameraYStart;
    v.camZ = config.cameraDistanceZ;
    v.vignetteStrength = config.vignetteBase;
    v.radialBlur = 0.0f;
    v.blackoutAlpha = 0.0f;
    v.triggerImpactAudio = false;

    float tNod = config.animNodDuration;
    float tDive = tNod + config.animDiveDuration;
    float tImpact = tDive + config.animImpactDuration;

    if (tSeconds < tNod)
    {
        // Single Nod: Dip down then recover
        float u = tSeconds / tNod;
        float dip = sinf(u * PI);
        if (dip < 0) dip = 0;
        
        v.pitchDeg -= config.nodPitchDip * dip;
        v.camY -= config.nodHeadDrop * dip;
        v.vignetteStrength = config.vignetteBase + config.vignettePulse * dip;
        
        // Nod blackout: 0 -> 0.5 -> 0
        v.blackoutAlpha = 0.5f * dip;
    }
    else if (tSeconds < tDive)
    {
        float u = (tSeconds - tNod) / config.animDiveDuration;
        float eased = powf(u, config.diveCurvePower); 
        
        v.pitchDeg = Lerp(config.cameraPitchStart, config.diveTargetPitch, eased);
        v.camX = Lerp(config.cameraXStart, config.diveTargetX, eased);
        v.camY = Lerp(config.cameraYStart, config.diveTargetY, eased);
        v.camZ = Lerp(config.cameraDistanceZ, config.diveTargetZ, eased);
        
        v.radialBlur = config.radialBlurMax * u;
        v.vignetteStrength = config.vignetteBase + config.vignetteFallBoost * u;
        
        // Dive blackout: 0 -> 1.0
        v.blackoutAlpha = u;
    }
    else if (tSeconds < tImpact)
    {
        float b = (tSeconds - tDive) / config.animImpactDuration;
        float bounce = sinf(b * PI);
        
        v.pitchDeg = config.diveTargetPitch;
        v.camX = config.diveTargetX;
        v.camY = config.diveTargetY + config.impactBounceHeight * bounce;
        v.camZ = config.diveTargetZ;
        
        v.radialBlur = config.radialBlurMax * (1.0f - b);
        v.vignetteStrength = config.vignetteImpact;
        v.blackoutAlpha = 1.0f;
        
        if (!impactPlayed) v.triggerImpactAudio = true;
        impactPlayed = true;
    }
    else
    {
        v.pitchDeg = config.diveTargetPitch;
        v.camX = config.diveTargetX;
        v.camY = config.diveTargetY;
        v.camZ = config.diveTargetZ;
        v.radialBlur = 0.0f;
        v.vignetteStrength = config.vignetteImpact;
        v.blackoutAlpha = 1.0f;
    }

    return v;
}

void OpeningScene::PlayImpactAudio()
{
    if (!audioReady) return;
    if (thudSound.frameCount > 0) PlaySound(thudSound);
    if (rumbleSound.frameCount > 0) PlaySound(rumbleSound);
}

void OpeningScene::DrawMenuScene(const Camera &cam, int screenW, int screenH, bool showUI) const
{
    BeginMode3D(cam);
    
    // Update shader view position and light position if lighting is enabled
    if (lightingSetup && lightingShader.id > 0)
    {
        float cameraPos[3] = { cam.position.x, cam.position.y, cam.position.z };
        SetShaderValue(lightingShader, lightingShader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);
        
        // Update light position dynamically (for tweak mode)
        Light updatedLight = mainLight;
        updatedLight.position = {config.lightPosX, config.lightPosY, config.lightPosZ};
        updatedLight.color = {
            (unsigned char)Clamp(config.lightColorR, 0.0f, 255.0f),
            (unsigned char)Clamp(config.lightColorG, 0.0f, 255.0f),
            (unsigned char)Clamp(config.lightColorB, 0.0f, 255.0f),
            255
        };
        UpdateLightValues(lightingShader, updatedLight);
        
        // Update ambient light dynamically
        int ambientLoc = GetShaderLocation(lightingShader, "ambient");
        float ambientValue[4] = {
            config.ambientColorR / 255.0f * config.ambientIntensity,
            config.ambientColorG / 255.0f * config.ambientIntensity,
            config.ambientColorB / 255.0f * config.ambientIntensity,
            1.0f
        };
        SetShaderValue(lightingShader, ambientLoc, ambientValue, SHADER_UNIFORM_VEC4);
    }
    
    // Draw Table with lighting
    Vector3 scale = {config.tableWidth * config.modelScale, config.tableHeight * config.modelScale, config.tableDepth * config.modelScale};
    DrawModelEx(tableModel, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 0.0f, scale, WHITE);

    // Draw Volumetric Light Cone
    float coneHeight = config.lightConeTopY - config.lightConeBottomY;
    float coneCenterY = config.lightConeBottomY + coneHeight * 0.5f;
    
    // Enable blending for transparency
    rlDisableBackfaceCulling();
    Color coneCol = config.lightConeColor;
    coneCol.a = (unsigned char)Clamp(config.lightConeAlpha, 0.0f, 255.0f);
    DrawCylinder({0.0f, coneCenterY, 0.0f}, config.lightConeTopRadius, config.lightConeBottomRadius, coneHeight, 16, coneCol);
    rlEnableBackfaceCulling();

    EndMode3D();

    // Draw UI
    if (showUI)
    {
        const char *text = "Press ENTER to start";
        int fontSize = config.uiFontSize;
        int textWidth = MeasureText(text, fontSize);
        int x = (screenW - textWidth) / 2;
        int y = (int)(screenH * config.uiButtonYRatio);
        
        DrawText(text, x, y, fontSize, config.uiColorHover);
    }
}

// --- TWEAK MODE IMPLEMENTATION ---

struct TweakParam {
    std::string name;
    float* ptr;
    float step;
    float minVal;
    float maxVal;
    bool hidden; // Don't show in UI, but still save/load
};

std::vector<TweakParam> GetTweakParams(OpeningConfig& cfg) {
    return {
        // Table
        {"Table Width", &cfg.tableWidth, 0.05f, 0.1f, 5.0f, false},
        {"Table Depth", &cfg.tableDepth, 0.05f, 0.1f, 5.0f, false},
        {"Table Height", &cfg.tableHeight, 0.05f, 0.1f, 2.0f, false},
        {"Model Scale", &cfg.modelScale, 0.001f, 0.001f, 100.0f, false},
        
        // Volumetric Light Cone
        {"Cone Top Y", &cfg.lightConeTopY, 0.1f, 0.0f, 10.0f, false},
        {"Cone Bot Y", &cfg.lightConeBottomY, 0.1f, 0.0f, 10.0f, false},
        {"Cone Top Rad", &cfg.lightConeTopRadius, 0.01f, 0.0f, 2.0f, false},
        {"Cone Bot Rad", &cfg.lightConeBottomRadius, 0.05f, 0.0f, 5.0f, false},
        {"Cone Alpha", &cfg.lightConeAlpha, 1.0f, 0.0f, 255.0f, false},
        
        // Vignette
        {"Vignette Radius", &cfg.spotlightRadius, 0.01f, 0.1f, 2.0f, false},
        {"Vignette Alpha", &cfg.spotlightMaxAlpha, 1.0f, 0.0f, 255.0f, false},
        
        // Camera (visible)
        {"Cam FOV", &cfg.cameraFov, 1.0f, 10.0f, 120.0f, false},
        
        // Camera position (hidden but saved) - controlled by WASD
        {"Cam X Start", &cfg.cameraXStart, 0.1f, -10.0f, 10.0f, true},
        {"Cam Y Start", &cfg.cameraYStart, 0.1f, 0.0f, 10.0f, true},
        {"Cam Dist Z", &cfg.cameraDistanceZ, 0.1f, -10.0f, 10.0f, true},
        {"Cam Pitch", &cfg.cameraPitchStart, 1.0f, -90.0f, 90.0f, true},
        
        // Real Lighting System
        {"Light Pos Y", &cfg.lightPosY, 0.1f, 0.0f, 10.0f, false},
        {"Light Color R", &cfg.lightColorR, 5.0f, 0.0f, 255.0f, false},
        {"Light Color G", &cfg.lightColorG, 5.0f, 0.0f, 255.0f, false},
        {"Light Color B", &cfg.lightColorB, 5.0f, 0.0f, 255.0f, false},
        {"Light Intensity", &cfg.lightIntensity, 0.05f, 0.0f, 5.0f, false},
        {"Ambient R", &cfg.ambientColorR, 1.0f, 0.0f, 100.0f, false},
        {"Ambient G", &cfg.ambientColorG, 1.0f, 0.0f, 100.0f, false},
        {"Ambient B", &cfg.ambientColorB, 1.0f, 0.0f, 100.0f, false},
        {"Ambient Int", &cfg.ambientIntensity, 0.01f, 0.0f, 1.0f, false}
    };
}

void OpeningScene::UpdateTweakMode(Camera &cam)
{
    bool moving = false;
    float moveSpeed = 0.005f; // Even slower default
    if (IsKeyDown(KEY_LEFT_CONTROL)) moveSpeed = 0.001f; // Super slow mode

    // Camera Movement (Free Cam)
    if (IsKeyDown(KEY_W)) { UpdateCameraPro(&cam, {moveSpeed, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f); moving = true; }
    if (IsKeyDown(KEY_S)) { UpdateCameraPro(&cam, {-moveSpeed, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f); moving = true; }
    if (IsKeyDown(KEY_A)) { UpdateCameraPro(&cam, {0.0f, moveSpeed, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f); moving = true; }
    if (IsKeyDown(KEY_D)) { UpdateCameraPro(&cam, {0.0f, -moveSpeed, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f); moving = true; }
    
    // World Up/Down
    if (IsKeyDown(KEY_SPACE)) { cam.position.y += moveSpeed; cam.target.y += moveSpeed; moving = true; }
    if (IsKeyDown(KEY_LEFT_SHIFT)) { cam.position.y -= moveSpeed; cam.target.y -= moveSpeed; moving = true; }

    // Mouse Look
    Vector2 mouseDelta = GetMouseDelta();
    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
    {
        UpdateCameraPro(&cam, {0.0f, 0.0f, 0.0f}, {mouseDelta.x * 0.05f, mouseDelta.y * 0.05f, 0.0f}, 0.0f); // Reduced mouse sensitivity
        moving = true;
    }

    // Sync Config FROM Camera if moving
    if (moving)
    {
        config.cameraXStart = cam.position.x;
        config.cameraYStart = cam.position.y;
        config.cameraDistanceZ = cam.position.z;
        
        Vector3 forward = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
        config.cameraPitchStart = asinf(forward.y) * RAD2DEG;
    }

    // Parameter Selection (skip hidden params)
    auto params = GetTweakParams(config);
    if (IsKeyPressed(KEY_DOWN)) {
        do {
            tweakSelection = (tweakSelection + 1) % params.size();
        } while (params[tweakSelection].hidden);
    }
    if (IsKeyPressed(KEY_UP)) {
        do {
            tweakSelection = (tweakSelection - 1 + params.size()) % params.size();
        } while (params[tweakSelection].hidden);
    }

    // Parameter Adjustment
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_LEFT))
    {
        float dir = IsKeyDown(KEY_RIGHT) ? 1.0f : -1.0f;
        float speed = IsKeyDown(KEY_LEFT_CONTROL) ? 0.2f : 1.0f;
        TweakParam& p = params[tweakSelection];
        *p.ptr += p.step * dir * speed;
        if (*p.ptr < p.minVal) *p.ptr = p.minVal;
        if (*p.ptr > p.maxVal) *p.ptr = p.maxVal;

        // Sync Camera TO Config if tweaking camera params
        if (p.name.find("Cam") != std::string::npos)
        {
            cam.position = {config.cameraXStart, config.cameraYStart, config.cameraDistanceZ};
            float pitchRad = config.cameraPitchStart * DEG2RAD;
            Vector3 forward = {0.0f, sinf(pitchRad), cosf(pitchRad)};
            cam.target = Vector3Add(cam.position, forward);
            cam.fovy = config.cameraFov;
        }
    }
}

void OpeningScene::DrawTweakUI() const
{
    auto params = GetTweakParams(const_cast<OpeningConfig&>(config));
    int screenH = GetScreenHeight();
    int startY = 20;
    int startX = 20;
    
    DrawText("TWEAK MODE (F4 to Exit, F5 to Save)", startX, startY, 20, YELLOW);
    startY += 30;
    
    // Calculate max lines that fit on screen
    int lineHeight = 22;
    int maxLines = (screenH - startY - 50) / lineHeight; // Leave space at bottom
    
    // Count only visible params
    int visibleCount = 0;
    for (const auto& p : params) {
        if (!p.hidden) visibleCount++;
    }
    
    // Draw sections with scrolling (only count visible params)
    const char* sections[] = {"TABLE", "CONE", "VIGNETTE", "CAMERA", "LIGHTING"};
    int sectionBreaks[] = {0, 4, 9, 11, 12, visibleCount};
    
    // Calculate which items to show based on selection (only visible)
    int visibleSelection = 0;
    for (int i = 0; i <= tweakSelection && i < (int)params.size(); i++) {
        if (!params[i].hidden) {
            if (i == tweakSelection) break;
            visibleSelection++;
        }
    }
    
    int scrollOffset = 0;
    if (visibleSelection >= maxLines / 2)
    {
        scrollOffset = visibleSelection - maxLines / 2;
        if (scrollOffset + maxLines > visibleCount)
            scrollOffset = visibleCount - maxLines;
    }
    if (scrollOffset < 0) scrollOffset = 0;
    
    int currentSection = 0;
    int lineY = startY;
    int linesDrawn = 0;
    int visibleIdx = 0;
    
    for (size_t i = 0; i < params.size() && linesDrawn < maxLines; ++i)
    {
        if (params[i].hidden) continue; // Skip hidden params
        
        // Check if we need to draw a section header
        while (currentSection < 5 && visibleIdx >= sectionBreaks[currentSection + 1])
            currentSection++;
            
        if (visibleIdx == sectionBreaks[currentSection])
        {
            if (visibleIdx > 0 && linesDrawn > 0 && visibleIdx >= scrollOffset) 
            {
                lineY += 8;
                linesDrawn++;
                if (linesDrawn >= maxLines) break;
            }
            if (visibleIdx >= scrollOffset) {
                DrawText(sections[currentSection], startX, lineY, 16, SKYBLUE);
                lineY += lineHeight;
                linesDrawn++;
                if (linesDrawn >= maxLines) break;
            }
        }
        
        if (visibleIdx >= scrollOffset) {
            Color c = (i == (size_t)tweakSelection) ? GREEN : LIGHTGRAY;
            const char* text = TextFormat("  %s: %.2f", params[i].name.c_str(), *params[i].ptr);
            DrawText(text, startX, lineY, 18, c);
            lineY += lineHeight;
            linesDrawn++;
        }
        visibleIdx++;
    }
    
    // Show scroll indicator
    if (visibleCount > maxLines)
    {
        const char* scrollHint = TextFormat("[%d/%d]", visibleSelection + 1, visibleCount);
        DrawText(scrollHint, startX, screenH - 50, 16, YELLOW);
    }
    
    // Show hints
    DrawText("Arrows: Navigate | Left/Right: Adjust | Ctrl: Fine", startX, screenH - 30, 16, GRAY);
}

void OpeningScene::SaveConfig() const
{
    std::cout << "Attempting to save config..." << std::endl;
    std::ofstream file("opening_config.txt");
    if (file.is_open())
    {
        auto params = GetTweakParams(const_cast<OpeningConfig&>(config));
        for (const auto& p : params)
        {
            file << p.name << "=" << *p.ptr << "\n";
        }
        file.close();
        std::cout << "Config saved to opening_config.txt" << std::endl;
    }
}

void OpeningScene::LoadConfig()
{
    std::ifstream file("opening_config.txt");
    if (file.is_open())
    {
        auto params = GetTweakParams(config);
        std::string line;
        while (std::getline(file, line))
        {
            size_t eq = line.find('=');
            if (eq != std::string::npos)
            {
                std::string key = line.substr(0, eq);
                float val = std::stof(line.substr(eq + 1));
                for (auto& p : params)
                {
                    if (p.name == key)
                    {
                        *p.ptr = val;
                        break;
                    }
                }
            }
        }
        file.close();
        std::cout << "Config loaded." << std::endl;
    }
}

void OpeningScene::DrawSpotlightMask(int screenW, int screenH) const
{
    Vector2 center = {(float)screenW * 0.5f, (float)screenH * 0.5f};
    float radius = (float)screenW * config.spotlightRadius;
    for (int i = 0; i < config.spotlightLayers; ++i)
    {
        float t = (float)i / (float)config.spotlightLayers;
        Color c = {0, 0, 0, (unsigned char)(config.spotlightMaxAlpha * t)};
        DrawCircleGradient((int)center.x, (int)center.y, radius * (1.0f + config.spotlightLayerGrow * t), Fade(BLACK, 0.0f), c);
    }
}

void OpeningScene::DrawVignetteAndBlackout(int screenW, int screenH, float vignetteStrength, float blackoutAlpha) const
{
    int vignette = (int)(screenW * 0.18f);
    Color edge = Fade(BLACK, Clamp(vignetteStrength, 0.0f, 1.0f));
    
    DrawRectangleGradientH(0, 0, vignette, screenH, edge, Fade(edge, 0.0f));
    DrawRectangleGradientH(screenW - vignette, 0, vignette, screenH, Fade(edge, 0.0f), edge);
    DrawRectangleGradientV(0, 0, screenW, vignette, edge, Fade(edge, 0.0f));
    DrawRectangleGradientV(0, screenH - vignette, screenW, vignette, Fade(edge, 0.0f), edge);
    
    if (blackoutAlpha > 0.0f)
    {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, Clamp(blackoutAlpha, 0.0f, 1.0f)));
    }
}
