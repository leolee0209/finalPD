#pragma once
#include "me.hpp"
#include <vector>
#include "object.hpp"
#include <raylib.h>
#include "uiManager.hpp"
#include "updateContext.hpp"
// Base class for all attack controllers, attack instances stored in attackManager
class AttackController
{
protected:
    // Pointer to the entity that spawned this attack
    AttackController(Entity *_spawnedBy) : spawnedBy(_spawnedBy) {}

public:
    Entity *const spawnedBy;                    // The entity (player or enemy) that spawned this attack
    virtual void update(UpdateContext &uc) = 0; // Pure virtual function to update the attack
    virtual void draw() {}                      // Virtual function for custom rendering
    virtual std::vector<Entity *> getEntities() = 0;
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
    void spawnProjectile(MahjongTileType tile, Texture2D *texture, Rectangle tile_rect);
    void addProjectiles(std::vector<Projectile> &&new_projectiles);

    // Updates the state of the attack (e.g., moves projectiles, checks for activation)
    void update(UpdateContext &uc) override;

    bool isActivated() const { return this->activated; }

    // Returns a list of objects representing the projectiles for rendering or collision detection
    std::vector<Object *> obj()
    {
        std::vector<Object *> ret;
        for (auto &p : this->projectiles)
            ret.push_back(&p.obj());
        return ret;
    }
    std::vector<Entity *> getEntities() override
    {
        std::vector<Entity *> ret;
        for (auto &p : this->projectiles)
            ret.push_back(&p);
        return ret;
    }
};

class TripletAttack : public AttackController
{
private:
    enum TripletState
    {
        READY,
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

    void spawnProjectile(MahjongTileType tile, Texture2D *texture, Rectangle tile_rect);
    void addProjectiles(std::vector<Projectile> &&new_projectiles);
    void update(UpdateContext &uc) override;

    // Return projectile objects and connector objects for rendering
    std::vector<Object *> obj()
    {
        std::vector<Object *> ret;
        for (auto &p : this->projectiles)
            ret.push_back(&p.obj());
        for (auto &c : this->connectors)
            ret.push_back(&c);
        return ret;
    }
    std::vector<Entity *> getEntities() override
    {
        std::vector<Entity *> ret;
        for (auto &p : this->projectiles)
            ret.push_back(&p);
        return ret;
    }
};

class SingleTileAttack : public AttackController
{
private:
    std::vector<Projectile> projectiles;
    std::vector<float> lifetime;
    static constexpr float shootHoriSpeed = 30;
    static constexpr float shootVertSpeed = 10;

public:
    SingleTileAttack(Entity *_spawnedBy) : AttackController(_spawnedBy) {}

    void spawnProjectile(MahjongTileType tile, Texture2D *texture, Rectangle tile_rect);
    void update(UpdateContext &uc) override;

    std::vector<Projectile> takeLastProjectiles(int n);
    std::vector<Projectile> &getProjectiles() { return this->projectiles; }

    std::vector<Object *> obj()
    {
        std::vector<Object *> ret;
        for (auto &p : this->projectiles)
            ret.push_back(&p.obj());
        return ret;
    }
    std::vector<Entity *> getEntities() override
    {
        std::vector<Entity *> ret;
        for (auto &p : this->projectiles)
            ret.push_back(&p);
        return ret;
    }
};