#pragma once
#include <raylib.h>
#include <constant.hpp>
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
    void triggerMeleeSwing(float durationSeconds);
    float getMeleeSwingAmount() const;
    void beginMeleeWindup(float durationSeconds);
    bool isInMeleeWindup() const;
    void addCameraShake(float magnitude, float durationSeconds);
    void addCameraFovKick(float magnitude, float durationSeconds);
    void applyKnockback(const Vector3 &pushVelocity, float durationSeconds, float lift = 0.0f);
    bool damage(DamageResult &dResult);

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