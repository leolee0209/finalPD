#pragma once
#include <raylib.h>
#include <constant.hpp>
#include <vector>
#include "object.hpp"
#include "mycamera.hpp"
#include "uiManager.hpp"
#include "updateContext.hpp"

struct DamageResult;
/**
 * @brief Category used to classify entities for filtered queries.
 */
enum EntityCategory
{
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_PROJECTILE,
    ENTITY_ALL,
};
/**
 * @brief Base class for all entities in the game (player, enemies, projectiles).
 *
 * Stores the physical `Object` (`o`) used for collisions, plus kinematic
 * state: `position`, `velocity`, `direction` and `grounded` flag.
 *
 * Implementations should override `UpdateBody(UpdateContext&)` and may call
 * `ApplyPhysics(...)` to perform common gravity, drag, acceleration,
 * integration and floor collision logic.
 */
class Entity
{
protected:
    Object o;          // The object representing the entity's physical body
    Vector3 position;  // Current position of the entity
    Vector3 velocity;  // Current velocity of the entity
    Vector3 direction; // Current movement direction of the entity
    bool grounded;     // Whether the entity is on the ground
    /**
     * @brief Parameters that control the shared physics integration.
     *
     * These values are used by `ApplyPhysics` to tune gravity, ground/air
     * deceleration, maximum horizontal speed/acceleration, and floor level.
     */
    struct PhysicsParams
    {
        bool useGravity = true;
        float gravity = GRAVITY;
        float decelGround = FRICTION;
        float decelAir = AIR_DRAG;
        float maxSpeed = MAX_SPEED; // desired max horizontal speed
        float maxAccel = MAX_ACCEL; // max acceleration (units/sec^2)
        float floorY = 0.0f;        // floor Y-level for ground collision
        bool iterativeCollisionResolve = false; // whether to call resolveCollision
        float zeroThreshold = 0.0f; // if >0 use as absolute threshold for zeroing hvel, else uses maxSpeed*0.01
    };
    
    static void resolveCollision(Entity *e, UpdateContext &uc);
    /**
     * @brief Apply common physics integration to an entity.
     *
     * This helper performs gravity application, ground/air drag, optional
     * acceleration along `entity->direction`, position integration,
     * optional iterative collision resolution, and floor clamping.
     *
     * @param e Entity to update (modified in-place).
     * @param uc Frame update context (scene, player, input snapshot).
     * @param p Physics tuning parameters.
     */
    static void ApplyPhysics(Entity *e, UpdateContext &uc, const PhysicsParams &p);

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
    void setPosition(const Vector3 &newPos)
    {
        this->position = newPos;
        this->o.pos = newPos;
        this->o.UpdateOBB();
    }

    /**
     * @brief Per-frame body update.
     *
     * Override this in derived classes to implement behavior (AI, player
     * input, projectiles). Use `ApplyPhysics()` to perform the low-level
     * integration and collision resolution when appropriate.
     *
     * @param uc Frame update context with scene and input snapshot.
     */
    virtual void UpdateBody(UpdateContext& uc) = 0;
    /**
     * @brief Category of this entity (enemy/player/projectile).
     *
     * Derived classes must override to allow filtered entity queries.
     */
    virtual EntityCategory category() const = 0;
};

// Class representing an enemy entity
/**
 * @brief Enemy entity with simple AI and animation state.
 *
 * Enemy::UpdateBody implements chasing the player, smoothing rotation and
 * basic run/bob animations. Damage is applied via `damage(DamageResult&)`.
 */
class Enemy : public Entity
{
private:
    int health; // Enemy's health
    
    // Animation state variables
    float runTimer;
    float runLerp;
    Vector3 facingDirection; // Added for smooth turning
    float knockbackTimer = 0.0f;
    float hitTilt = 0.0f;

protected:
    struct MovementSettings
    {
        float maxSpeed = 3.0f;
        Vector3 facingHint = {0.0f, 0.0f, 0.0f};
        bool lockToGround = false;
        float leanScale = 2.0f;
        float maxLeanAngle = 35.0f;
        bool enableLean = true;
        bool enableBobAndSway = true;
        float maxAccel = MAX_ACCEL;
        float decelGround = FRICTION;
        float decelAir = AIR_DRAG;
        float zeroThreshold = -1.0f;
        bool overrideHorizontalVelocity = false;
        Vector3 forcedHorizontalVelocity = {0.0f, 0.0f, 0.0f};
    };

    void UpdateCommonBehavior(UpdateContext &uc, const Vector3 &desiredDirection, float deltaSeconds, const MovementSettings &settings);
    bool isKnockbackActive() const { return this->knockbackTimer > 0.0f; }
    float computeSupportHeightForRotation(const Quaternion &rotation) const;
    void snapToGroundWithRotation(const Quaternion &rotation);

public:
    // Default constructor initializes the enemy with default values
    Enemy()
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = true;
        health = MAX_HEALTH_ENEMY;
        
        // Initialize animation state
        runTimer = 0.0f;
        runLerp = 0.0f;
        facingDirection = {0.0f, 0.0f, 1.0f}; // Default forward
    }

    // Updates the enemy's body (movement, jumping, etc.)
    void UpdateBody(UpdateContext& uc) override;
    bool damage(DamageResult &dResult);
    void applyKnockback(const Vector3 &pushVelocity, float durationSeconds, float lift = 0.0f);
    // Identify this entity as an enemy for filtered queries
    EntityCategory category() const override;
    Vector3 getFacingDirection() const { return this->facingDirection; }
    virtual void gatherObjects(std::vector<Object *> &out) const;
    int getHealth() const { return this->health; }
    int getMaxHealth() const { return MAX_HEALTH_ENEMY; }
    float getHealthPercent() const
    {
        if (MAX_HEALTH_ENEMY <= 0)
            return 0.0f;
        float percent = static_cast<float>(this->health) / static_cast<float>(MAX_HEALTH_ENEMY);
        if (percent < 0.0f)
            percent = 0.0f;
        if (percent > 1.0f)
            percent = 1.0f;
        return percent;
    }
};

class ChargingEnemy : public Enemy
{
private:
    enum class ChargeState
    {
        Approaching,
        Windup,
        Charging,
        Recover
    };

    ChargeState state = ChargeState::Approaching;
    float stateTimer = 0.0f;
    Vector3 chargeDirection = {0.0f, 0.0f, 1.0f};

    float stopDistance = 25.0f;
    float windupDuration = 1.2f;
    float chargeDuration = 0.7f;
    float recoverDuration = 2.5f;
    float approachSpeed = 4.0f;
    float chargeSpeed = 45.0f;
    float chargeSpinMinDegPerSec = 240.0f;
    float chargeSpinMaxDegPerSec = 1200.0f;
    float chargeSpinAngleDeg = 0.0f;
    float chargePoseAngleDeg = 0.0f;
    float poseAngularVelocityDegPerSec = 0.0f;
    float poseFallAccelerationDegPerSec2 = 900.0f;
    float poseRiseAccelerationDegPerSec2 = 900.0f;
    float poseMaxAngularVelocityDegPerSec = 1440.0f;
    float chargeDamage = 25.0f;
    float chargeKnockbackForce = 18.0f;
    bool appliedChargeDamage = false;

    bool updatePoseTowards(float targetAngleDeg, float deltaSeconds);

public:
    ChargingEnemy() = default;
    void UpdateBody(UpdateContext &uc) override;
};

class ShooterEnemy : public Enemy
{
private:
    enum class Phase
    {
        FindPosition,
        Shooting
    };

    struct MovementCommand
    {
        Vector3 direction{0.0f, 0.0f, 0.0f};
        float speed = 0.0f;
    };

    struct Bullet
    {
        Vector3 position;
        Vector3 velocity;
        float radius;
        float remainingLife;
        Object visual;
    };

    struct BulletPattern
    {
        int bulletCount = 1;      // Number of bullets to fire
        float arcDegrees = 0.0f;  // Spread arc in degrees (0 = single direction)
    };

    std::vector<Bullet> bullets;
    BulletPattern bulletPattern;  // Current bullet pattern configuration
    float fireCooldown = 0.0f;
    float fireInterval = 2.0f;
    float bulletSpeed = 25.0f;
    float bulletRadius = 0.3f;
    float bulletLifetime = 6.0f;
    float bulletDamage = 8.0f;
    float muzzleHeight = 3.0f;
    float maxFiringDistance = 45.0f;
    float retreatDistance = 20.0f;
    int maxActiveBullets = 6;
    int strafeDirection = 1;
    float losRepositionTimer = 0.0f;
    float approachSpeed = 6.0f;
    float retreatSpeed = 6.5f;
    float strafeSpeed = 4.0f;
    float strafeSwitchInterval = 1.2f;
    Phase phase = Phase::FindPosition;
    Vector3 losRepositionGoal = {0.0f, 0.0f, 0.0f};
    bool hasRepositionGoal = false;
    float repositionCooldown = 0.0f;
    float repositionCooldownDuration = 0.7f;

    bool findShotDirection(UpdateContext &uc, Vector3 &outDir) const;
    bool hasLineOfFire(const Vector3 &start, const Vector3 &end, UpdateContext &uc, float probeRadius) const;
    void spawnBullet(const Vector3 &origin, const Vector3 &dir);
    void updateBullets(UpdateContext &uc, float deltaSeconds);
    MovementCommand FindMovement(UpdateContext &uc, const Vector3 &toPlayer, float distance, bool hasLineOfSight, float deltaSeconds);
    bool isWithinPreferredRange(float distance) const;
    void HandleShooting(float deltaSeconds, const Vector3 &muzzle, const Vector3 &aimDir, bool hasAim);
    bool HasLineOfSightFromPosition(const Vector3 &origin, UpdateContext &uc) const;
    bool SelectRepositionGoal(UpdateContext &uc, const Vector3 &planarToPlayer, float distanceToPlayer);

public:
    ShooterEnemy();
    void UpdateBody(UpdateContext &uc) override;
    void gatherObjects(std::vector<Object *> &out) const override;
    void setBulletPattern(int bulletCount, float arcDegrees)
    {
        this->bulletPattern.bulletCount = bulletCount;
        this->bulletPattern.arcDegrees = arcDegrees;
    }
};
// Class representing the player character
/**
 * @brief Player-controlled entity.
 *
 * `Me` owns a `MyCamera` for first-person rendering and handles player
 * movement via `applyPlayerMovement`. Use `getLookRotation()` to obtain
 * the current camera look direction for aiming.
 */
class Me : public Entity
{
private:
    int health;      // Player's health
    MyCamera camera; // Camera associated with the player
    float meleeSwingTimer = 0.0f;
    float meleeSwingDuration = 0.25f;
    float meleeWindupTimer = 0.0f;
    float knockbackTimer = 0.0f;
    float shootSlowTimer = 0.0f;
    float shootSlowFactor = 1.0f;
    float colliderWidth = 1.2f;
    float colliderDepth = 1.2f;
    float colliderHeight = 1.8f;

    // New: Struct to hold player input state
    

    // New: Private method for core player movement logic
    void applyPlayerMovement(UpdateContext& uc);
    float getColliderHalfHeight() const { return this->colliderHeight * 0.5f; }

public:
    // Default constructor initializes the player with default values
    Me()
    {
        position = {0.0f, this->getColliderHalfHeight(), 0.0f};
        velocity = {0};
        direction = {0};
        grounded = true;
        health = MAX_HEALTH_ME;

        this->o.setAsBox({this->colliderWidth, this->colliderHeight, this->colliderDepth});
        this->o.pos = this->position;
        this->o.visible = false;
        this->o.UpdateOBB();

        camera = MyCamera(this->position, this->getColliderHalfHeight()); // Initialize the camera with the player's position
    }
    // Overrides Entity's UpdateBody
    void UpdateBody(UpdateContext& uc) override;

    // Updates the player's camera based on movement and actions
    void UpdateCamera(UpdateContext& uc);
    void triggerMeleeSwing(float durationSeconds);
    float getMeleeSwingAmount() const;
    void beginMeleeWindup(float durationSeconds);
    bool isInMeleeWindup() const;
    void addCameraShake(float magnitude, float durationSeconds);
    void addCameraFovKick(float magnitude, float durationSeconds);
    void applyKnockback(const Vector3 &pushVelocity, float durationSeconds, float lift = 0.0f);
    bool damage(DamageResult &dResult);
    int getHealth() const { return this->health; }
    void applyShootSlow(float slowFactor, float durationSeconds);
    float getMovementMultiplier() const;

    // Getter for the player's camera
    const Camera &getCamera() { return this->camera.getCamera(); }
    const Camera &getCamera() const { return this->camera.getCamera(); }

    // Getter for the player's look rotation (used for aiming and movement direction)
    Vector2 &getLookRotation() { return this->camera.lookRotation; }
    const Vector2 &getLookRotation() const { return this->camera.lookRotation; }
    // Identify this entity as the player
    EntityCategory category() const override;
};



// Class representing a projectile (e.g., bullets, missiles)
/**
 * @brief Physics-driven projectile spawned by attacks.
 *
 * Projectiles use friction/airDrag parameters and perform collision checks
 * against the scene. They do not accelerate themselves (handled by
 * the spawner/attack controller).
 */
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
    // Identify this entity as a projectile
    EntityCategory category() const override;
};

typedef struct DamageResult{
    float damage;
    CollisionResult &cResult;
    DamageResult(float _damage, CollisionResult &_cResult) : damage(_damage), cResult(_cResult) {};
} DamageResult;