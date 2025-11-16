#include "me.hpp"
#include "constant.hpp"
#include "raymath.h"

void Me::UpdateBody(float rot, char side, char forward, bool jumpPressed, bool crouchHold)
{
    Vector2 input = {(float)side, (float)-forward};

    if ((side != 0) && (forward != 0))
        input = Vector2Normalize(input);

    float delta = GetFrameTime();

    if (!this->grounded)
        this->velocity.y -= GRAVITY * delta;

    if (this->grounded && jumpPressed)
    {
        this->velocity.y = JUMP_FORCE;
        this->grounded = false;

        // Sound can be played at this moment
        // SetSoundPitch(fxJump, 1.0f + (GetRandomValue(-100, 100)*0.001));
        // PlaySound(fxJump);
    }

    //calculate the direction facing
    Vector3 front = {sinf(rot), 0.f, cosf(rot)};
    Vector3 right = {cosf(-rot), 0.f, sinf(-rot)};

    Vector3 desiredDir = {
        input.x * right.x + input.y * front.x,
        0.0f,
        input.x * right.z + input.y * front.z,
    };
    this->dir = Vector3Lerp(this->dir, desiredDir, CONTROL * delta);

    float decel = (this->grounded ? FRICTION : AIR_DRAG);
    Vector3 hvel = {this->velocity.x * decel, 0.0f, this->velocity.z * decel};

    float hvelLength = Vector3Length(hvel); // Magnitude
    if (hvelLength < (MAX_SPEED * 0.01f))
        hvel = {0};

    // This is what creates strafing
    float speed = Vector3DotProduct(hvel, this->dir);

    // Whenever the amount of acceleration to add is clamped by the maximum acceleration constant,
    // a Player can make the speed faster by bringing the direction closer to horizontal velocity angle
    // More info here: https://youtu.be/v3zT3Z5apaM?t=165
    float maxSpeed = (crouchHold ? CROUCH_SPEED : MAX_SPEED);
    float accel = Clamp(maxSpeed - speed, 0.f, MAX_ACCEL * delta);
    hvel.x += this->dir.x * accel;
    hvel.z += this->dir.z * accel;

    this->velocity.x = hvel.x;
    this->velocity.z = hvel.z;

    this->position.x += this->velocity.x * delta;
    this->position.y += this->velocity.y * delta;
    this->position.z += this->velocity.z * delta;

    // Fancy collision system against the floor
    if (this->position.y <= 0.0f)
    {
        this->position.y = 0.0f;
        this->velocity.y = 0.0f;
        this->grounded = true; // Enable jumping
    }
}