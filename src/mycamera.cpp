#include "mycamera.hpp"
#include <raylib.h>
#include <raymath.h>
#include <cmath>
void MyCamera::UpdateCamera(char sideway, char forward, bool crouching, Vector3 playerCenter, float colliderHalfHeight, bool isGrounded, float swingAmount)
{
    float delta = GetFrameTime();
    this->headLerp = Lerp(this->headLerp, (crouching ? CROUCH_HEIGHT : STAND_HEIGHT), 20.0f * delta);
    float footY = playerCenter.y - colliderHalfHeight;
    this->camera.position = {
        playerCenter.x,
        footY + (BOTTOM_HEIGHT + this->headLerp),
        playerCenter.z,
    };

    float targetFov = 60.0f;
    if (isGrounded && ((forward != 0) || (sideway != 0)))
    {
        headTimer += delta * 3.0f;
        walkLerp = Lerp(walkLerp, 1.0f, 10.0f * delta);
        targetFov = 55.0f;
    }
    else
    {
        walkLerp = Lerp(walkLerp, 0.0f, 10.0f * delta);
        targetFov = 60.0f;
    }

    if (this->fovKickTimer > 0.0f && this->fovKickDuration > 0.0f)
    {
        this->fovKickTimer = fmaxf(0.0f, this->fovKickTimer - delta);
        float t = this->fovKickTimer / this->fovKickDuration;
        targetFov += this->fovKickMagnitude * t;
    }

    this->camera.fovy = Lerp(this->camera.fovy, targetFov, 5.0f * delta);

    lean.x = Lerp(lean.x, sideway * 0.02f, 10.0f * delta);
    lean.y = Lerp(lean.y, forward * 0.015f, 10.0f * delta);

    this->swingLean = swingAmount * 0.08f;
    this->swingRoll = swingAmount * 0.12f;
    this->swingLift = swingAmount * 0.05f;

    UpdateCameraFPS();

    if (this->shakeTimer > 0.0f)
    {
        this->shakeTimer = fmaxf(0.0f, this->shakeTimer - delta);
        float normalized = (this->shakeDuration > 0.0f) ? (this->shakeTimer / this->shakeDuration) : 0.0f;
        float strength = this->shakeMagnitude * normalized * normalized; // ease-out
        float shakeX = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * strength;
        float shakeY = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * strength * 0.7f;
        float shakeZ = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * strength;
        Vector3 offset = {shakeX, shakeY, shakeZ};
        this->camera.position = Vector3Add(this->camera.position, offset);
        this->camera.target = Vector3Add(this->camera.target, offset);
    }
}

void MyCamera::UpdateCameraFPS()
{
    const Vector3 up = {0.0f, 1.0f, 0.0f};
    const Vector3 targetOffset = {0.0f, 0.0f, -1.0f};

    // Left and right
    Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, lookRotation.x);

    // Clamp view up
    float maxAngleUp = Vector3Angle(up, yaw);
    maxAngleUp -= 0.001f; // Avoid numerical errors
    if (-(lookRotation.y) > maxAngleUp)
    {
        lookRotation.y = -maxAngleUp;
    }

    // Clamp view down
    float maxAngleDown = Vector3Angle(Vector3Negate(up), yaw);
    maxAngleDown *= -1.0f;  // Downwards angle is negative
    maxAngleDown += 0.001f; // Avoid numerical errors
    if (-(lookRotation.y) < maxAngleDown)
    {
        lookRotation.y = -maxAngleDown;
    }

    // Up and down
    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

    // Rotate view vector around right axis
    float pitchAngle = -lookRotation.y - lean.y;
    pitchAngle = Clamp(pitchAngle, -PI / 2 + 0.0001f, PI / 2 - 0.0001f); // Clamp angle so it doesn't go past straight up or straight down
    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    // Head animation
    // Rotate up direction around forward axis
    float headSin = sinf(headTimer * PI);
    float headCos = cosf(headTimer * PI);
    const float stepRotation = 0.01f;
    this->camera.up = Vector3RotateByAxisAngle(up, pitch, headSin * stepRotation + lean.x + this->swingRoll);

    // Camera BOB
    const float bobSide = 0.1f;
    const float bobUp = 0.15f;
    Vector3 bobbing = Vector3Scale(right, headSin * bobSide);
    bobbing.y = fabsf(headCos * bobUp);

    this->camera.position = Vector3Add(this->camera.position, Vector3Scale(bobbing, walkLerp));
    Vector3 swingOffset = Vector3Scale(right, this->swingLean);
    swingOffset.y += this->swingLift;
    this->camera.position = Vector3Add(this->camera.position, swingOffset);
    this->camera.target = Vector3Add(this->camera.position, pitch);
}

void MyCamera::addShake(float magnitude, float durationSeconds)
{
    if (durationSeconds <= 0.0f || magnitude <= 0.0f)
        return;
    this->shakeDuration = durationSeconds;
    this->shakeTimer = durationSeconds;
    this->shakeMagnitude = fmaxf(this->shakeMagnitude, magnitude);
}

void MyCamera::resetShake()
{
    this->shakeTimer = 0.0f;
    this->shakeDuration = 0.0f;
    this->shakeMagnitude = 0.0f;
    this->fovKickTimer = 0.0f;
    this->fovKickDuration = 0.0f;
    this->fovKickMagnitude = 0.0f;
}

void MyCamera::addFovKick(float magnitude, float durationSeconds)
{
    if (durationSeconds <= 0.0f || magnitude <= 0.0f)
        return;
    this->fovKickDuration = durationSeconds;
    this->fovKickTimer = durationSeconds;
    this->fovKickMagnitude = magnitude;
}
