#pragma once
#include <raylib.h>
#include <constant.hpp>
#include "scene.hpp"
class Entity
{
protected:
    Vector3 position;
    Vector3 velocity;
    Vector3 dir;
    bool grounded;
    int health;

public:
    Entity()
    {
        position = {0};
        velocity = {0};
        dir = {0};
        grounded = true;
    }
    float px() { return this->position.x; }
    float py() { return this->position.y; }
    float pz() { return this->position.z; }
    bool isGrounded() { return this->grounded; }
};

class Me : public Entity
{
public:
    Me()
    {
        position = {0};
        velocity = {0};
        dir = {0};
        grounded = true;
        health = MAX_HEALTH_ME;
    }
    void UpdateBody(float rot, char side, char forward, bool jumpPressed, bool crouchHold, bool isCollided);
};
class Enemy : public Entity
{
private:
    Object o;
public:
    Enemy()
    {
        position = {0};
        velocity = {0};
        dir = {0};
        grounded = true;
        health = MAX_HEALTH_ENEMY;
    }
    void UpdateBody(float rot, char side, char forward, bool jumpPressed, bool crouchHold);
};