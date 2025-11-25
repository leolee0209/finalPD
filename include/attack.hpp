#pragma once
#include <vector>
#include <raylib.h>
#include "uiManager.hpp"
#include "updateContext.hpp"
#include "me.hpp"

class Object;
// Base class for all attack controllers, attack instances stored in attackManager
class AttackController
{
protected:
    // Pointer to the entity that spawned this attack
    AttackController(Entity *_spawnedBy) : spawnedBy(_spawnedBy) {}

public:
    virtual ~AttackController() {} // ensure proper polymorphic destruction and allow vtable emission
    Entity *const spawnedBy;
    virtual void update(UpdateContext &uc) = 0;
    virtual std::vector<Entity *> getEntities() = 0;
};

class ThousandTileAttack : public AttackController
{
private:
    int count;
    std::vector<Projectile> projectiles;
    static constexpr float shootHoriSpeed = 30;
    static constexpr float shootVertSpeed = 10;

    // Modes for the tile-based attack that used to live in separate classes
    enum Mode
    {
        MODE_IDLE,
        MODE_NORMAL,
        MODE_THOUSAND_FINAL,     // converge to a dest (was ThousandAttack final)
        MODE_TRIPLET_CONNECTING, // spawn connectors (was TripletAttack connecting)
        MODE_TRIPLET_FINAL,
    } mode = MODE_NORMAL;

    // Thousand-mode data (converge to dest)
    static constexpr float thousandFinalVel = 30.0f;
    Vector3 thousandDest;
    bool thousandActivated = false;

    // Triplet-mode connector objects
    std::vector<Object> connectors;
    std::vector<float> connectorForward; // per-connector progress / rotation state
    float connectingTimer = 0.0f;
    static constexpr float connectorThickness = 0.15f;
    static constexpr float tripletFinalHeight = 6.0f;
    static constexpr float tripletGrowthRate = 0.15f;
    void updateTriplet(UpdateContext &uc); // internal triplet update helper

public:
    void spawnProjectile(UpdateContext &uc);
    // Helpers
    void startThousandMode(); // move existing ThousandAttack::activateFinal / endFinal logic here
    bool thousandEndFinal();  // return true when finished
    void startTripletMode();  // move TripletAttack connector logic here

    ThousandTileAttack(Entity *_spawnedBy) : AttackController(_spawnedBy), count(0) {}
    void update(UpdateContext &uc) override;

    // Expose projectile objects + connectors for Scene rendering (Scene will draw them)
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