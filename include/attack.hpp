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
 * @brief Simple tile-based attack controller for basic shooting.
 *
 * Fires single projectiles horizontally from the camera/entity direction.
 * Used for normal attacks without special combo modes.
 */
class BasicTileAttack : public AttackController
{
private:
    std::vector<Projectile> projectiles;
    float cooldownRemaining = 0.0f;
    static constexpr float shootSpeed = 70.0f;
    static constexpr float projectileSize = 0.025f;
    static constexpr float projectileDamage = 10.0f;
    static constexpr float cooldownDuration = 0.5f;
    static constexpr float movementSlowDuration = 0.3f;
    static constexpr float movementSlowFactor = 0.4f; // Reduces speed to 40% of normal

public:
    BasicTileAttack(Entity *_spawnedBy) : AttackController(_spawnedBy) {}
    bool canShoot() const { return this->cooldownRemaining <= 0.0f; }
    void update(UpdateContext &uc) override;
    void spawnProjectile(UpdateContext &uc);
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
        MODE_THOUSAND_FINAL,     // converge to a dest (was ThousandAttack final)
        MODE_TRIPLET_CONNECTING, // spawn connectors (was TripletAttack connecting)
        MODE_TRIPLET_FINAL,
    } mode = MODE_IDLE;

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
    float getCooldownPercent() const;

private:
    struct EffectVolume
    {
        Object area;
        float remainingLife;
    };

    struct TileIndicator
    {
        Object sprite;
        bool active = false;
        bool launched = false;
        float opacity = 0.0f;
        float travelProgress = 0.0f;
        Vector3 startPos = {0.0f, 0.0f, 0.0f};
        Vector3 targetPos = {0.0f, 0.0f, 0.0f};
        Vector3 forward = {0.0f, 0.0f, 0.0f};
    } tileIndicator;

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
    static constexpr float pushDamage = 14.0f;
    static constexpr float effectLifetime = 0.2f;
    static constexpr float effectHeight = 3.5f;
    static constexpr float effectYOffset = 0.5f;
    static constexpr float cameraShakeMagnitude = 0.6f;
    static constexpr float cameraShakeDuration = 0.25f;

    static constexpr float indicatorWidth = 0.6f;
    static constexpr float indicatorHeight = 0.75f;
    static constexpr float indicatorThickness = 0.15f;
    static constexpr float indicatorHoldDistance = 0.8f;
    static constexpr float indicatorYOffset = 1.4f;
    static constexpr float indicatorTravelDuration = 0.12f;
    static constexpr float indicatorStartOpacity = 0.0f;
    static constexpr float indicatorEndOpacity = 0.5f;

    std::vector<EffectVolume> effectVolumes;

    Vector3 getForwardVector() const;
    struct ViewBasis
    {
        Vector3 position;
        Vector3 forward;
    };
    ViewBasis getIndicatorViewBasis() const;
    void setIndicatorPose(const Vector3 &position, const Vector3 &forward);
    bool pushEnemies(UpdateContext &uc, EffectVolume &volume);
    EffectVolume buildEffectVolume(const Vector3 &origin, const Vector3 &forward) const;
    void performStrike(UpdateContext &uc);
    void requestPlayerWindupLock();
    void providePlayerFeedback(bool hit);

    void initializeTileIndicator(UpdateContext &uc);
    void updateTileIndicator(UpdateContext &uc, float deltaSeconds);
    void launchTileIndicator(const ViewBasis &view);
    void deactivateTileIndicator();
};

/**
 * @brief Short, high-speed dash that moves the player along their input vector.
 */
class DashAttack : public AttackController
{
public:
    explicit DashAttack(Entity *_spawnedBy) : AttackController(_spawnedBy) {}

    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override { return {}; }
    void trigger(UpdateContext &uc);
    float getCooldownPercent() const;

private:
    float cooldownRemaining = 0.0f;
    float activeRemaining = 0.0f;
    Vector3 dashDirection = {0.0f, 0.0f, 0.0f};

    static constexpr float dashSpeed = 70.0f;
    static constexpr float dashDuration = 0.25f;
    static constexpr float dashCooldown = 1.0f;

    static constexpr float dashFovKick = 50.0f;
    static constexpr float dashFovKickDuration = 0.3f;

    Vector3 computeDashDirection(const UpdateContext &uc) const;
    void applyDashImpulse(Me *player, UpdateContext &uc);
    Vector3 computeCollisionAdjustedVelocity(Me *player, UpdateContext &uc, float desiredSpeed);
};

class DotBombAttack : public AttackController
{
public:
    explicit DotBombAttack(Entity *_spawnedBy);
    ~DotBombAttack() override;

    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override;
    std::vector<Object *> obj();
    bool trigger(UpdateContext &uc, MahjongTileType tile);
    float getCooldownPercent() const;

private:
    struct Bomb
    {
        Projectile projectile;
        bool exploded = false;
        float flightTimeRemaining = 4.0f;
        float explosionTimer = 0.0f;
        Object explosionFx;
        bool fxActive = false;
        Vector3 explosionOrigin{0.0f, 0.0f, 0.0f};
        Object explosionSprite;
        bool spriteActive = false;
    };

    std::vector<Bomb> bombs;

    static constexpr float projectileSpeed = 50.0f;
    static constexpr float projectileLift = 6.0f;
    static constexpr float projectileDrag = 0.99f;
    static constexpr float projectileFriction = 0.92f;
    static constexpr float muzzleHeight = 1.6f;
    static constexpr float muzzleForwardOffset = 0.8f;
    static constexpr float projectileRadius = 0.45f;
    static constexpr float projectileHeight = 1.4f;
    static constexpr float projectileLength = 3.5f;

    static constexpr float explosionLifetime = 0.35f;
    static constexpr float explosionStartRadius = 3.0f;
    static constexpr float explosionEndRadius = 10.0f;
    static constexpr float explosionHeight = 5.0f;
    static constexpr float explosionDamage = 25.0f;
    static constexpr float explosionKnockback = 55.0f;
    static constexpr float explosionKnockbackDuration = 0.6f;
    static constexpr float explosionLift = 30.0f;
    static constexpr float explosionSpriteDepth = 0.15f;
    static constexpr float explosionSpriteStartSize = 1.5f;
    static constexpr float explosionSpriteEndSize = 6.5f;
    static constexpr float cooldownDuration = 2.0f;
    float cooldownRemaining = 0.0f;

    void startExplosion(Bomb &bomb, const Vector3 &origin, UpdateContext &uc);
    void applyExplosionEffects(const Vector3 &origin, UpdateContext &uc);
    static void retainExplosionTexture();
    static void releaseExplosionTexture();
    void updateExplosionBillboard(Bomb &bomb, UpdateContext &uc, float normalizedProgress);

    static Texture2D explosionTexture;
    static bool explosionTextureLoaded;
    static int explosionTextureUsers;
    static constexpr const char *explosionTexturePath = "wabbit_alpha.png";
};