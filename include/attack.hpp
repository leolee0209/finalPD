#pragma once
#include <vector>
#include <array>
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
    float activeCooldownModifier = 1.0f; // 1.0 = normal, 0.4 = 40% of normal (faster shooting)
    
    // Tweakable Bamboo/Ranger attack parameters
    static constexpr float shootSpeed = 70.0f;             // Projectile speed
    static constexpr float projectileSize = 0.025f;        // Size multiplier
    static constexpr float projectileDamage = 10.0f;       // Base damage
    static constexpr float cooldownDuration = 0.5f;        // Fire rate cooldown
    static constexpr float movementSlowDuration = 0.3f;    // Slow duration after shot
    static constexpr float movementSlowFactor = 0.4f;      // Reduces speed to 40% of normal
    static constexpr float horizontalSpinSpeed = 450.0f;     // How fast tile spins horizontally (tweakable)
    static constexpr float trailWidth = 0.3f;              // Width of wind trail (tweakable)
    static constexpr float trailLength = 2.0f;             // Length of trail behind tile (tweakable)

public:
    BasicTileAttack(Entity *_spawnedBy) : AttackController(_spawnedBy) {}
    bool canShoot() const { return this->cooldownRemaining <= 0.0f; }
    void setCooldownModifier(float modifier) { this->activeCooldownModifier = modifier; }
    void resetCooldownModifier() { this->activeCooldownModifier = 1.0f; }
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

/**
 * @brief Rapid-fire attack triggered by three same-number bamboo tiles.
 *
 * Temporarily reduces the cooldown of BasicTileAttack for a duration,
 * allowing faster shooting. The attack itself has a longer cooldown
 * than the effect duration.
 */
class BambooTripleAttack : public AttackController
{
public:
    explicit BambooTripleAttack(Entity *_spawnedBy) : AttackController(_spawnedBy) {}

    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override { return {}; }
    void trigger(UpdateContext &uc);
    float getCooldownPercent() const;
    bool isActive() const { return this->effectRemaining > 0.0f; }
    float getReducedCooldown() const;

private:
    float cooldownRemaining = 0.0f;
    float effectRemaining = 0.0f;

    // Effect duration is 3.0s, cooldown is 5.0s (cooldown > effect)
    static constexpr float effectDuration = 3.0f;
    static constexpr float cooldownDuration = 5.0f;
    
    // Original cooldown is 0.5s, reduced to 0.2s during effect
    static constexpr float normalCooldown = 0.5f;
    static constexpr float reducedCooldown = 0.2f;
};

class BambooBombAttack : public AttackController
{
public:
    explicit BambooBombAttack(Entity *_spawnedBy);
    ~BambooBombAttack() override;

    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override;
    std::vector<Object *> obj();
    bool trigger(UpdateContext &uc, TileType tile);
    float getCooldownPercent() const;

private:
    struct Bomb
    {
        Projectile projectile;
        bool exploded = false;
        float flightTimeRemaining = 4.0f;
        float explosionTimer = 0.0f;
        float tumbleRotation = 0.0f;  // Current tumble angle
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
    static constexpr float tumbleSpeed = 720.0f;  // Heavy end-over-end tumbling (degrees/sec)

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

/**
 * @brief Fan Shot shotgun-style attack - fires 5 projectiles in horizontal spread.
 */
class FanShotAttack : public AttackController
{
public:
    explicit FanShotAttack(Entity *_spawnedBy) : AttackController(_spawnedBy) {}

    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override;
    std::vector<Object *> obj();
    bool trigger(UpdateContext &uc);
    float getCooldownPercent() const;

private:
    std::vector<Projectile> projectiles;
    float cooldownRemaining = 0.0f;

    // Camera recoil state
    bool recoilActive = false;
    float recoilTimer = 0.0f;
    float originalPitch = 0.0f;

    static constexpr int spreadCount = 5;           // Number of projectiles
    static constexpr float spreadAngle = 40.0f;     // Total spread angle in degrees
    static constexpr float projectileSpeed = 65.0f; // Fast shotgun pellets
    static constexpr float projectileDamage = 8.0f; // Lower damage per pellet
    static constexpr float projectileSize = 0.022f; // Slightly smaller than normal
    static constexpr float muzzleHeight = 1.6f;
    static constexpr float cooldownDuration = 1.2f; // Longer cooldown for shotgun
    static constexpr float recoilPitchKick = 0.3f;  // Camera pitch recoil
    static constexpr float recoilDuration = 0.3f;   // Total recoil time
    static constexpr float recoilKickTime = 0.1f;   // Fast upward kick
        static constexpr float recoilKickSpeed = 8.0f;      // Speed of upward recoil kick
        static constexpr float recoilRecoverySpeed = 4.0f;  // Speed of camera recovery (slower)
};

/**
 * @brief Dragon's Claw melee attack - 3-hit combo with spirit tile visuals.
 *
 * Creates animated spirit tiles that slash from different positions with rotation.
 * Animation mimics MeleePushAttack with side positioning and forward rotation.
 */
class DragonClawAttack : public AttackController
{
private:
    struct SlashEffect
    {
        Object spiritTile;
        Vector3 startPos = {0.0f, 0.0f, 0.0f};
        Vector3 swipeOrigin = {0.0f, 0.0f, 0.0f};
        float animationProgress = 0.0f;
        float lifetime = 0.0f;
        int comboIndex = 0; // 0 = first slash, 1 = second, 2 = third
        bool hasHit = false;
        float rotationProgress = 0.0f; // 0 to 1, how far through the forward rotation
    };

    struct ArcCurve
    {
        // Local space control points (x=right, y=up, z=forward)
        Vector3 p0;
        Vector3 p1;
        Vector3 p2;
        Vector3 p3;
    };

    std::vector<SlashEffect> activeSlashes;
    std::vector<Object> debugArcPoints;
    std::array<ArcCurve, 3> arcCurves;
    std::array<ArcCurve, 3> defaultArcCurves;
    Vector3 lastForward = {0.0f, 0.0f, -1.0f};
    Vector3 lastRight = {1.0f, 0.0f, 0.0f};
    Vector3 lastBasePos = {0.0f, 0.0f, 0.0f};
    float comboTimer = 0.0f;
    int comboCount = 0; // 0, 1, or 2 (which slash in the combo)
    float cooldownRemaining = 0.0f;
    
    // === Tweakable Animation Timing Parameters ===
    static constexpr float attackDuration = 0.35f;       // Total duration of the swing animation
    static constexpr float windupDuration = 0.07f;       // Windup phase (20% of animation)
    static constexpr float strikeDuration = 0.175f;      // Strike phase (50% of animation)
    static constexpr float followThruDuration = 0.105f;  // Follow-through phase (30% of animation)
    
    static constexpr float attackCooldown = 0.5f;        // Cooldown between combos
    static constexpr float comboResetTime = 1.5f;        // Time to reset combo if no hit
    
    // === Tweakable Visual Parameters ===
    static constexpr float spiritTileWidth = 1.0f;       // Width of the spirit tile
    static constexpr float spiritTileHeight = 1.25f;     // Height of the spirit tile
    static constexpr float spiritTileThickness = 0.3f;  // Thickness of the spirit tile
    static constexpr float spiritTileOpacity = 0.5f;     // Base opacity (0.0 to 1.0, stays translucent)
    static constexpr float slashDamage = 30.0f;          // Damage per slash
    static constexpr int arcDebugSamples = 24;           // How many debug particles to show on arc
    static constexpr float debugParticleRadius = 0.08f;  // Visual size of arc debug particles
    
    // === Tweakable Movement Parameters ===
    static constexpr float startDistance = -1.0f;        // Starting distance (negative = behind player)
    static constexpr float strikeDistance = 2.0f;        // Peak distance during strike (in front of player)
    static constexpr float endDistance = 0.5f;           // Final distance after follow-through
    
    static constexpr float arcHeight = 1.2f;             // Height of the arc trajectory
    static constexpr float sideOffset = 1.5f;            // How far to side for diagonal strikes
    
    static constexpr float playerStepDistance = 0.0f;    // How far player steps forward per swing
    
    // === Tweakable Rotation Parameters ===
    static constexpr float windupRotation = -0.3f;       // Back-tilt angle during windup (radians)
    static constexpr float strikeRotation = 1.0f;        // Forward rotation during strike (radians)
    static constexpr float followThruRotation = 0.5f;    // Settle rotation after strike (radians)
    
    static constexpr float cameraShakeMagnitude = 0.3f;  // Camera shake intensity on hit
    static constexpr float cameraShakeDuration = 0.15f;  // Camera shake duration
    static constexpr const char *arcSaveFilename = "dragon_claw_arcs.txt"; // persisted tweak file
    
    static bool tweakModeEnabled;
    static int tweakSelectedCombo;

public:
    DragonClawAttack(Entity *_spawnedBy);
    
    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override { return {}; }
    std::vector<Object *> obj();
    void spawnSlash(UpdateContext &uc);
    bool canAttack() const { return cooldownRemaining <= 0.0f; }
    float getCooldownPercent() const { return cooldownRemaining / attackCooldown; }
    // Tweak helpers (callable from main loop)
    void handleTweakHotkeys();
    void refreshDebugArc(const Vector3 &forward, const Vector3 &right, const Vector3 &basePos);
    
private:
    Vector3 getSlashOrientation(int comboIndex, float progress, const Vector3 &forward, const Vector3 &right) const;
    Vector3 getSlashPosition(int comboIndex, float progress, const Vector3 &forward, const Vector3 &right, const Vector3 &basePos) const;
    float getRotationAmount(float progress) const;
    float easeInCubic(float t) const { return t * t * t; }
    float easeOutCubic(float t) const { float f = t - 1.0f; return f * f * f + 1.0f; }
    void checkSlashHits(SlashEffect &slash, UpdateContext &uc);
    float mapProgressToArcT(float progress) const;
    Vector3 evalArcPoint(const ArcCurve &curve, float t, const Vector3 &forward, const Vector3 &right, const Vector3 &basePos) const;
    Vector3 evalArcTangent(const ArcCurve &curve, float t, const Vector3 &forward, const Vector3 &right) const;
    void resetArcDefaults();
    void nudgeArc(int comboIndex, const Vector3 &deltaP1, const Vector3 &deltaP2);
    void nudgeArcPoint(int comboIndex, int pointIndex, const Vector3 &delta);
    bool saveArcCurves() const;
    bool loadArcCurves();
public:
    static bool isTweakModeEnabled() { return tweakModeEnabled; }
    static int getTweakSelectedCombo() { return tweakSelectedCombo; }
    static void toggleTweakMode() { tweakModeEnabled = !tweakModeEnabled; }
    static void setTweakMode(bool enabled) { tweakModeEnabled = enabled; }
    static void applyTweakCamera(const Me &player, Camera &cam);
    static void drawTweakHud(const Me &player);
};

/**
 * @brief Arcane Orb basic attack - homing projectile with sine-wave motion.
 *
 * Fires a slow, homing projectile that follows enemies with sine-wave tracking.
 */
class ArcaneOrbAttack : public AttackController
{
private:
    struct OrbProjectile
    {
        Vector3 position;
        Vector3 lastDirection;  // Direction from previous frame
        Vector3 targetPos;
        Entity *targetEnemy = nullptr;
        float lifetime = 0.0f;
        float sineWavePhase = 0.0f;
        Object orbObj;
        bool active = true;
        
        static constexpr float maxLifetime = 8.0f;
        static constexpr float baseSpeed = 10.0f;       // Base movement speed (tweakable)
        static constexpr float sineWaveAmplitude = 1.5f; // Vertical sine motion amplitude
        static constexpr float sineWaveFrequency = 2.0f; // Speed of sine wave oscillation
        static constexpr float trackingBlend = 0.25f;    // 0.0 = pure last direction, 1.0 = pure target tracking
        static constexpr float damage = 12.0f;
        static constexpr float orbRadius = 0.4f;
        static constexpr float searchRadius = 35.0f;    // Max distance to search for enemies
    };

    std::vector<OrbProjectile> activeOrbs;
    float cooldownRemaining = 0.0f;
    
    // Tweakable animation parameters
    static constexpr float orbSize = 0.5f;         // Size of the orb visual
    static constexpr float orbSpinSpeed = 3.0f;    // How fast the orb rotates on itself
    static constexpr float muzzleHeight = 1.6f;    // Height to spawn orb from
    static constexpr float cooldownDuration = 0.6f; // Time between shots

public:
    ArcaneOrbAttack(Entity *_spawnedBy) : AttackController(_spawnedBy) {}
    
    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override { return {}; }
    std::vector<Object *> obj();
    void spawnOrb(UpdateContext &uc);
    bool canShoot() const { return cooldownRemaining <= 0.0f; }
    float getCooldownPercent() const { return cooldownRemaining / cooldownDuration; }
    
private:
    Entity *findNearestEnemy(UpdateContext &uc, const Vector3 &position, float searchRadius) const;
    void updateOrbMovement(OrbProjectile &orb, UpdateContext &uc, float deltaSeconds);
    void checkOrbHits(OrbProjectile &orb, UpdateContext &uc);
};

/** @brief Seismic Slam - leap and ground pound with arc motion and camera control.
 *
 * Player follows a Bézier arc, faces tangent direction, slams ground with shockwave.
 * Uses same tweak framework as Dragon Claw for arc editing.
 */
class SeismicSlamAttack : public AttackController
{
public:
    explicit SeismicSlamAttack(Entity *_spawnedBy);

    void update(UpdateContext &uc) override;
    std::vector<Entity *> getEntities() override { return {}; }
    std::vector<Object *> obj();
    bool trigger(UpdateContext &uc);
    float getCooldownPercent() const;

    // Tweak helpers (callable from main loop)
    void handleTweakHotkeys();
    void refreshDebugArc(const Vector3 &forward, const Vector3 &right, const Vector3 &basePos);
    static bool isTweakModeEnabled() { return tweakModeEnabled; }
    static void toggleTweakMode() { tweakModeEnabled = !tweakModeEnabled; }
    static void setTweakMode(bool enabled) { tweakModeEnabled = enabled; }
    static void applyTweakCamera(const Me &player, Camera &cam);
    static void drawTweakHud(const Me &player);

    struct ArcCurve
    {
        Vector3 p0; // Start (ground)
        Vector3 p1; // First control (upward arc)
        Vector3 p2; // Second control (apex)
        Vector3 p3; // End (landing)
    };

private:

    enum SlamState
    {
        SLAM_IDLE,
        SLAM_LEAP,      // Player follows arc upward
        SLAM_DESCEND,   // Player descends to ground
        SLAM_IMPACT,    // Ground impact and shockwave
        SLAM_RECOVERY   // Brief recovery period
    };

    SlamState state = SLAM_IDLE;
    float stateTimer = 0.0f;
    float animationProgress = 0.0f; // 0 to 1 for arc interpolation
    Vector3 leapStartPos = {0.0f, 0.0f, 0.0f};
    Vector3 savedVelocity = {0.0f, 0.0f, 0.0f};
    Vector3 lastForward = {0.0f, 0.0f, -1.0f};
    Vector3 lastRight = {1.0f, 0.0f, 0.0f};
    Vector3 lastBasePos = {0.0f, 0.0f, 0.0f};
    Object shockwaveRing;
    bool shockwaveActive = false;
    float shockwaveTimer = 0.0f;
    std::vector<Object> debugArcPoints;

    ArcCurve arcCurve;
    ArcCurve defaultArcCurve;
    float cooldownRemaining = 0.0f;

    // Timing parameters
    static constexpr float leapDuration = 0.6f;      // Time to reach apex
    static constexpr float descendDuration = 0.4f;   // Time to fall to ground
    static constexpr float impactDuration = 0.3f;    // Shockwave expansion time
    static constexpr float recoveryDuration = 0.2f;  // Brief lock after landing
    static constexpr float cooldownDuration = 3.0f;  // Long cooldown for ultimate

    // Damage and physics
    static constexpr float slamDamage = 40.0f;
    static constexpr float slamKnockback = 60.0f;
    static constexpr float slamKnockbackDuration = 0.8f;
    static constexpr float slamLift = 35.0f;
    static constexpr float shockwaveStartRadius = 2.0f;
    static constexpr float shockwaveEndRadius = 12.0f;
    static constexpr float shockwaveHeight = 1.5f;
    static constexpr float stunDuration = 1.0f;

    // Camera control
    static constexpr float windupLookUpAngle = 45.0f * DEG2RAD;  // Look up during leap
    static constexpr float impactLookDownAngle = 60.0f * DEG2RAD; // Look down at impact
    static constexpr float cameraTransitionSpeed = 3.0f;          // How fast camera tilts
    static constexpr float cameraRecoverySpeed = 1.5f;            // Slower recovery to neutral
    static constexpr float cameraShakeMagnitude = 1.2f;           // Strong shake on impact
    static constexpr float cameraShakeDuration = 0.4f;

    // Arc parameters (local space: x=right, y=up, z=forward)
    static constexpr float arcForwardDistance = 8.0f;  // How far forward to leap
    static constexpr float arcApexHeight = 5.0f;       // Peak height of leap
    static constexpr float gravityShape = 1.0f;        // 0 = linear speed, 1 = fast start/end, slow apex

    // Tweak system
    static constexpr int arcDebugSamples = 32;
    static constexpr float debugParticleRadius = 0.12f;
    static constexpr const char *arcSaveFilename = "seismic_slam_arc.txt";
    static bool tweakModeEnabled;

    void performLeap(UpdateContext &uc);
    void updateLeap(UpdateContext &uc, float delta);
    void updateDescend(UpdateContext &uc, float delta);
    void performImpact(UpdateContext &uc);
    void updateImpact(UpdateContext &uc, float delta);
    void updateRecovery(UpdateContext &uc, float delta);
    void applyShockwaveDamage(UpdateContext &uc);
    void updateCameraLook(UpdateContext &uc, float targetPitch);
    void restoreCameraControl(UpdateContext &uc);

    Vector3 evalArcPoint(const ArcCurve &curve, float t, const Vector3 &forward, const Vector3 &right, const Vector3 &basePos) const;
    Vector3 evalArcTangent(const ArcCurve &curve, float t, const Vector3 &forward, const Vector3 &right) const;
    void resetArcDefaults();
    void nudgeArc(const Vector3 &deltaP1, const Vector3 &deltaP2);
    void nudgeArcPoint(int pointIndex, const Vector3 &delta);
    bool saveArcCurve() const;
    bool loadArcCurve();
};