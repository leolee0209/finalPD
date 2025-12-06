#pragma once
#include "raylib.h"
#include "rlights.hpp"
#include <vector>

struct OpeningConfig
{
    // Table (Standard Mahjong Table ~0.9m x 0.9m x 0.75m)
    float tableWidth = 1.0f;
    float tableDepth = 1.0f;
    float tableHeight = 0.75f; // Leg height
    float tableTopY = 0.75f;   // Surface Y
    float modelScale = 1.0f;   // Global scale multiplier for the model
    Color tableTopColor = {20, 70, 20, 255};
    Color tableSideColor = {40, 25, 10, 255}; // Dark wood
    
    // Lighting
    bool enableRealisticLighting = true;
    float lightPosX = 0.0f;
    float lightPosY = 2.5f;
    float lightPosZ = 0.0f;
    float lightColorR = 255.0f;
    float lightColorG = 200.0f;  // Warmer: reduced green
    float lightColorB = 140.0f;  // Warmer: reduced blue
    float lightIntensity = 1.2f; // Slightly brighter
    float ambientColorR = 40.0f; // Warmer ambient
    float ambientColorG = 35.0f;
    float ambientColorB = 30.0f;
    float ambientIntensity = 0.15f;

    // Camera Start
    float cameraFov = 75.0f;       // Wider FOV
    float cameraXStart = 0.0f;     // Centered
    float cameraDistanceZ = -1.3f; // Further back to see table edge
    float cameraYStart = 1.25f;    // Eye level sitting
    float cameraPitchStart = -25.0f; // Looking down at tiles

    // Volumetric Light (Cone from ceiling)
    float lightConeTopY = 2.5f;       // Bulb height
    float lightConeBottomY = 0.75f;   // Table surface
    float lightConeTopRadius = 0.02f; // Bulb point
    float lightConeBottomRadius = 0.8f; // Spread on table
    float lightConeAlpha = 80.0f; // Alpha for volumetric light
    Color lightConeColor = {255, 240, 180, 255}; // Base color

    // UI Button
    float uiButtonYRatio = 0.8f; // Y position as ratio of screen height
    int uiFontSize = 40;
    Color uiColorNormal = {200, 200, 200, 255};
    Color uiColorHover = {255, 215, 0, 255}; // Gold

    // Spotlight / vignette
    float spotlightRadius = 0.4f; // Wider relative to screen
    int spotlightLayers = 8;
    float spotlightLayerGrow = 1.2f;
    float spotlightMaxAlpha = 180.0f; // Darker room
    float vignetteBase = 0.4f;
    float vignettePulse = 0.1f;
    float vignetteFallBoost = 0.2f;
    float vignetteImpact = 0.85f; // Very dark on impact

    // --- ANIMATION CONFIGURATION ---
    
    // Timings (Seconds)
    float animNodDuration = 2.5f;    // Time for the single nod
    float animDiveDuration = 1.6f;   // Time for the fall to table
    float animImpactDuration = 0.5f; // Time for bounce/settle
    float animSleepDelay = 1.0f;     // Wait time before loading

    // Nod Phase (The "fighting sleep" motion)
    float nodPitchDip = 12.0f;       // How many degrees head drops during nod
    float nodHeadDrop = 0.02f;       // How many meters head drops during nod

    // Dive Phase (The "faceplant")
    float diveTargetY = 0.80f;       // Final Y (Head on table)
    float diveTargetZ = -0.1f;        // Final Z (Forward on table - positive = towards camera)
    float diveTargetX = 0.0f;        // Final X (Side position)
    float diveTargetPitch = -90.0f;  // Final Pitch (Face down)
    float diveCurvePower = 2.8f;     // Easing power (higher = faster at end)

    // Impact Phase
    float impactBounceHeight = 0.04f; // Small bounce off table
    float blackoutFadeSpeed = 2.0f;   // Speed of fade to black

    // Blur
    float radialBlurMax = 2.0f; // Strong blur

    // Audio (procedural tones)
    float thudFreq = 60.0f; // Lower thud
    float thudDuration = 0.2f;
    float thudVolume = 0.9f;
    float thudDecay = 15.0f;

    float rumbleFreq = 30.0f;
    float rumbleDuration = 1.0f;
    float rumbleVolume = 0.7f;
    float rumbleDecay = 4.0f;
};

struct TransitionVisuals
{
    float pitchDeg = 0.0f;
    float camX = 0.0f;
    float camY = 0.0f;
    float camZ = 0.0f;
    float radialBlur = 0.0f;
    float vignetteStrength = 0.0f;
    float blackoutAlpha = 0.0f;
    bool triggerImpactAudio = false;
    bool overrideCamera = false; // If false, use main.cpp's camera system
};

class OpeningScene
{
  public:
    explicit OpeningScene(const OpeningConfig &cfg);
    bool Init();
    void Cleanup();
    void SetupLighting();

    TransitionVisuals EvaluateTransition(float tSeconds);
    void DrawMenuScene(const Camera &cam, int screenW, int screenH, bool showUI) const;
    void DrawSpotlightMask(int screenW, int screenH) const;
    void DrawVignetteAndBlackout(int screenW, int screenH, float vignetteStrength, float blackoutAlpha) const;
    void PlayImpactAudio();

    float GetCameraYStart() const { return config.cameraYStart; }
    float GetCameraXStart() const { return config.cameraXStart; }
    float GetCameraPitchStart() const { return config.cameraPitchStart; }
    float GetCameraDistanceZ() const { return config.cameraDistanceZ; }
    float GetCameraFov() const { return config.cameraFov; }
    
    float GetTotalDuration() const { 
        return config.animNodDuration + config.animDiveDuration + config.animImpactDuration + config.animSleepDelay; 
    }
    float GetImpactTime() const {
        return config.animNodDuration + config.animDiveDuration + config.animImpactDuration;
    }

    // Tweak Mode
    void UpdateTweakMode(Camera &cam);
    void DrawTweakUI() const;
    void SaveConfig() const;
    void LoadConfig();

  private:
    void RebuildTable();
    bool RayPlaneIntersection(const Ray &ray, float planeY, Vector3 &hit) const;
    Sound MakeTone(float freq, float duration, float volume, float decay) const;

    OpeningConfig config;
    Mesh tableMesh{};
    Model tableModel{};
    bool impactPlayed = false;

    bool audioReady = false;
    Sound thudSound{};
    Sound rumbleSound{};

    // Tweak State
    int tweakSelection = 0;
    
    // Lighting
    Shader lightingShader{};
    Light mainLight{};
    bool lightingSetup = false;
};
