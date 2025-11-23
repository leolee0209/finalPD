#pragma once
#include <raylib.h>
#include <constant.hpp>
#include "object.hpp"
#include "mycamera.hpp"
#include "uiManager.hpp"
#include "updateContext.hpp"
// Base class for all entities in the game (e.g., player, enemies, projectiles)
class Entity
{
protected:
    Object o;          // The object representing the entity's physical body
    Vector3 position;  // Current position of the entity
    Vector3 velocity;  // Current velocity of the entity
    Vector3 direction; // Current movement direction of the entity
    bool grounded;     // Whether the entity is on the ground

public:
    // Default constructor initializes the entity with default values
    Entity()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = true;
    }

    // Parameterized constructor initializes the entity with specific values
    Entity(Vector3 pos, Vector3 vel, Vector3 d, bool g)
    {
        position = pos;
        velocity = vel;
        direction = d;
        grounded = g;
    }

    // Getters for entity properties
    const Vector3 &pos() const { return this->position; }
    const Vector3 &vel() const { return this->velocity; }
    const Vector3 &dir() const { return this->direction; }
    Object &obj() { return this->o; }
    const Object &obj() const { return this->o; }
    bool isGrounded() const { return this->grounded; }

    // Setters for entity properties
    void setVelocity(const Vector3 &newVel) { this->velocity = newVel; }
    void setDirection(const Vector3 &newDir) { this->direction = newDir; }

    // Virtual function to update the entity's body (to be overridden by derived classes)
    virtual void UpdateBody(UpdateContext& uc) = 0;
};

// Class representing an enemy entity
class Enemy : public Entity
{
private:
    int health; // Enemy's health

public:
    // Default constructor initializes the enemy with default values
    Enemy()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = true;
        health = MAX_HEALTH_ENEMY;
    }

    // Updates the enemy's body (movement, jumping, etc.)
    void UpdateBody(UpdateContext& uc) override;
};
// Class representing the player character
class Me : public Entity
{
private:
    int health;      // Player's health
    MyCamera camera; // Camera associated with the player

    // New: Struct to hold player input state
    

    // New: Private method for core player movement logic
    void applyPlayerMovement(UpdateContext& uc);

public:
    // Default constructor initializes the player with default values
    Me()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = true;
        health = MAX_HEALTH_ME;

        camera = MyCamera(this->position); // Initialize the camera with the player's position
    }
    // Overrides Entity's UpdateBody
    void UpdateBody(UpdateContext& uc) override;

    // Updates the player's camera based on movement and actions
    void UpdateCamera(UpdateContext& uc);

    // Getter for the player's camera
    const Camera &getCamera() { return this->camera.getCamera(); }

    // Getter for the player's look rotation (used for aiming and movement direction)
    Vector2 &getLookRotation() { return this->camera.lookRotation; }
};



// Class representing a projectile (e.g., bullets, missiles)
class Projectile : public Entity
{
private:
    float friction; // Friction applied to the projectile when grounded
    float airDrag;  // Air drag applied to the projectile when in the air

public:
    MahjongTileType type;
    // Default constructor initializes the projectile with default values
    Projectile()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = false;
        friction = PROJECTILE_FRICTION;
        airDrag = PROJECTILE_AIR_DRAG;
    }

    // Parameterized constructor initializes the projectile with specific values
    Projectile(Vector3 pos, Vector3 vel, Vector3 d, bool g, Object o1, float fric, float aird, MahjongTileType _type)
    {
        this->position = pos;
        this->velocity = vel;
        this->direction = d;
        this->grounded = g;
        this->o = o1;
        this->friction = fric;
        this->airDrag = aird;
        this->type = _type;
    }

    // Updates the projectile's body (movement, gravity, etc.)
    void UpdateBody(UpdateContext& uc) override;
};