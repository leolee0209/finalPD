#pragma once
#include <raylib.h>
#include <constant.hpp>
#include "object.hpp"
#include "mycamera.hpp"
class Entity
{
protected:
    Object o;
    Vector3 position;
    Vector3 velocity;
    Vector3 direction;
    bool grounded;

public:
    Entity()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = true;
    }
    Entity(Vector3 pos, Vector3 vel, Vector3 d, bool g)
    {
        position = pos;
        velocity = vel;
        direction = d;
        grounded = g;
    }
    const Vector3 &pos() const { return this->position; }
    const Vector3 &vel() const { return this->velocity; }
    const Vector3 &dir() const { return this->direction; }
    const Object &obj() const { return this->o; }
    bool isGrounded() { return this->grounded; }
    virtual void UpdateBody() {};
};

class Me : public Entity
{
private:
    int health;
    MyCamera camera;

public:
    Me()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = true;
        health = MAX_HEALTH_ME;

        camera = MyCamera(this->position);
    }
    void UpdateBody(char side, char forward, bool jumpPressed, bool crouchHold);
    void UpdateCamera(char side, char forward, bool crouchHold);
    const Camera &getCamera() { return this->camera.getCamera(); }
    Vector2 &getLookRotation() { return this->camera.lookRotation; }
};
class Enemy : public Entity
{
private:
    int health;

public:
    Enemy()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = true;
        health = MAX_HEALTH_ENEMY;
    }
    void UpdateBody(float rot, char side, char forward, bool jumpPressed, bool crouchHold);
};

class Projectile : public Entity
{
private:
public:
    Projectile()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = false;
    }
    Projectile(Vector3 pos, Vector3 vel, Vector3 d, bool g, Object o1)
    {
        this->position = pos;
        this->velocity = vel;
        this->direction = d;
        this->grounded = g;
        this->o = o1;
    }
    void UpdateBody() override;
};