#pragma once
#include <raylib.h>

class Me
{
private:
    Vector3 position;
    Vector3 velocity;
    Vector3 dir;
    bool grounded;

public:
    Me()
    {
        position = {0};
        velocity = {0};
        dir = {0};
        grounded = true;
    }
    void UpdateBody(float rot, char side, char forward, bool jumpPressed, bool crouchHold);
    float px() { return this->position.x; }
    float py() { return this->position.y; }
    float pz() { return this->position.z; }
    bool isGrounded() { return this->grounded; }
};