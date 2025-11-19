#pragma once
#include <raylib.h>
#include "constant.hpp"
class MyCamera
{
private:
    Camera camera;
    float headLerp;
    float headTimer;
    float walkLerp;
    Vector2 lean;

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
    void UpdateCamera(char sideway, char forward, bool crouching, Vector3 playerPos, bool isGrounded);
};