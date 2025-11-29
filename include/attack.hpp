#pragma once
#include <vector>
#include <raylib.h>
#include "uiManager.hpp"
#include "updateContext.hpp"
#include "me.hpp"

class Object;

/**
 * @brief Abstract base for attack controllers that spawn and manage projectiles.
 *
 * Each AttackController is owned/managed by `AttackManager` and is bound to a
 * spawning `Entity` (stored in `spawnedBy`). Implement `update()` to advance
 * the controller and `getEntities()` to return any Entities (projectiles)
 * the controller manages.
 */
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

/**
 * @brief Tile-based attack controller used by players/enemies.
 *
 * Manages a list of `Projectile` instances and supports multiple modes
 * (normal firing, thousand-mode convergence, triplet connector mode).
 */
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

/**
 * @brief Short-range melee push attack that applies knockback to enemies.
 */
class MeleePushAttack : public AttackController
{
public:
    explicit MeleePushAttack(Entity *_spawnedBy) : AttackController(_spawnedBy) {}

    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override { return {}; }
    std::vector<Object *> obj() const;

    void trigger(UpdateContext &uc);

private:
    struct EffectVolume
    {
        Object area;
        float remainingLife;
    };

    float cooldownRemaining = 0.0f;
    float windupRemaining = 0.0f;
    bool pendingStrike = false;
    static constexpr float cooldownDuration = 0.8f;
    static constexpr float swingDuration = 0.25f;
    static constexpr float windupDuration = 0.18f;
    static constexpr float pushForce = 50.0f;
    static constexpr float pushRange = 10.0f;
    static constexpr float pushAngle = 70.0f * DEG2RAD;
    static constexpr float knockbackDuration = 0.45f;
    static constexpr float verticalLift = 2.5f;
    static constexpr float effectLifetime = 0.2f;
    static constexpr float effectHeight = 3.5f;
    static constexpr float effectYOffset = 0.5f;
    static constexpr float cameraShakeMagnitude = 0.6f;
    static constexpr float cameraShakeDuration = 0.25f;

    std::vector<EffectVolume> effectVolumes;

    Vector3 getForwardVector() const;
    bool pushEnemies(UpdateContext &uc, EffectVolume &volume);
    EffectVolume buildEffectVolume(const Vector3 &origin, const Vector3 &forward) const;
    void performStrike(UpdateContext &uc);
    void requestPlayerWindupLock();
    void providePlayerFeedback(bool hit);
};