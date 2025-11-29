#pragma once
#include <raylib.h>
#include "constant.hpp"

/**
 * @brief Simple first-person camera helper used by the player (`Me`).
 *
 * MyCamera maintains a `Camera` instance and smoothing state for head
 * bob/lean and look rotation. Call `UpdateCamera(...)` each frame to update
 * the camera transform based on player movement and crouch state.
 */
class MyCamera
{
private:
    Camera camera;
    float headLerp;
    float headTimer;
    float walkLerp;
    Vector2 lean;
    float swingRoll = 0.0f;
    float swingLean = 0.0f;
    float swingLift = 0.0f;
    float shakeTimer = 0.0f;
    float shakeDuration = 0.0f;
    float shakeMagnitude = 0.0f;

    void UpdateCameraFPS();

public:
    Vector2 lookRotation;
    MyCamera() {};
    MyCamera(Vector3 pos)
    {
        headLerp = STAND_HEIGHT;
        lookRotation = {0};
        headTimer = 0.0f;
        walkLerp = 0.0f;
        lean = {0};
        swingRoll = 0.0f;
        swingLean = 0.0f;
        swingLift = 0.0f;

        this->camera = {0};
        this->camera.fovy = 60.0f;
        this->camera.projection = CAMERA_PERSPECTIVE;
        this->camera.position = {
            pos.x,
            pos.y + (BOTTOM_HEIGHT + this->headLerp),
            pos.z,
        };

        UpdateCameraFPS();
    }
    const Camera &getCamera() { return this->camera; }

    /**
     * @brief Update internal camera transform from player state.
     *
     * @param sideway -1/0/1 for strafing input
     * @param forward -1/0/1 for forward/back input
     * @param crouching true if player is holding crouch
     * @param playerPos player's world position
     * @param isGrounded whether player is currently grounded
     * @param swingAmount normalized melee swing influence [0,1]
     */
    void UpdateCamera(char sideway, char forward, bool crouching, Vector3 playerPos, bool isGrounded, float swingAmount = 0.0f);

    /**
     * @brief Apply a short camera shake.
     */
    void addShake(float magnitude, float durationSeconds);
};