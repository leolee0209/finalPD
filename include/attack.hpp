#pragma once
#include "me.hpp"
#include <vector>
#include "object.hpp"
#include <raylib.h>

// Base class for all attack controllers, attack instances stored in attackManager
class AttackController
{
protected:
    // Pointer to the entity that spawned this attack
    AttackController(Entity *_spawnedBy) : spawnedBy(_spawnedBy) {}

public:
    Entity *const spawnedBy; // The entity (player or enemy) that spawned this attack
    virtual void update() = 0; // Pure virtual function to update the attack
    virtual void draw() {}     // Virtual function for custom rendering
};

// logic for the "Thousand Attack" ability
class ThousandAttack : public AttackController
{
private:
    static constexpr float finalVel = 30; // Final velocity of projectiles during the final phase
    static constexpr int activate = 3;    // Number of projectiles required to activate the final phase
    Vector3 dest;                         // Destination point for the projectiles in the final phase
    int count;                            // Number of projectiles currently active
    bool activated;                       // Whether the final phase has been activated
    std::vector<Projectile> projectiles;  // List of projectiles spawned by this attack

    // Activates the final phase of the attack, making all projectiles move toward a single point
    void activateFinal();

    // Checks if the final phase is complete (all projectiles have reached the destination)
    bool endFinal();

public:
    // Constructor initializes the attack with the spawning entity
    ThousandAttack(Entity *_spawnedBy) : AttackController(_spawnedBy)
    {
        this->count = 0;
        this->activated = false;
    }

    // Spawns a new projectile for this attack
    void spawnProjectile();

    // Updates the state of the attack (e.g., moves projectiles, checks for activation)
    void update() override;

    // Returns a list of objects representing the projectiles for rendering or collision detection
    std::vector<const Object *> obj()
    {
        std::vector<const Object *> ret;
        for (const auto &p : this->projectiles)
            ret.push_back(&p.obj());
        return ret;
    }
};

class TripletAttack : public AttackController
{
private:
    enum TripletState
    {
        READY,
        SHOOTING,
        CONNECTING,
        FINAL,
        DONE
    };
    TripletState state;
    Vector3 averageConnectorPos;
    bool isFalling;
    float fallingTimer;
    std::vector<Projectile> projectiles;
    std::vector<float> connectorForward;
    float connectingTimer;

    // Connector objects (long thin boxes) that visually connect projectiles.
    // These are created when transitioning to CONNECTING and exposed via obj().
    std::vector<Object> connectors;
    static constexpr float connectorThickness = 0.15f; // thickness of the connector boxes
    static constexpr float finalHeight = 6.0f;
    static constexpr float finalGrowthRate = 0.15f;
public:
    TripletAttack(Entity *_spawnedBy) : AttackController(_spawnedBy)
    {
        state = READY;
        connectingTimer = 0;
        isFalling = false;
        fallingTimer = 0.f;
    }

    void spawnProjectile();
    void update() override;

    // Return projectile objects and connector objects for rendering
    std::vector<const Object *> obj()
    {
        std::vector<const Object *> ret;
        for (const auto &p : this->projectiles)
            ret.push_back(&p.obj());
        for (const auto &c : this->connectors)
            ret.push_back(&c);
        return ret;
    }
};