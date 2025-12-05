#pragma once
#include <raylib.h>
#include <constant.hpp>
#include <vector>
#include "object.hpp"
class DialogBox;
#include "Inventory.hpp"
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
    int maxHealth = MAX_HEALTH_ENEMY; // Enemy's max health
    DialogBox *healthDialog = nullptr;
    TileType tileType = TileType::BAMBOO_1; // Associated mahjong tile type
    
    // Animation state variables
    float runTimer;
    float runLerp;
    Vector3 facingDirection; // Added for smooth turning
    float knockbackTimer = 0.0f;
    float hitTilt = 0.0f;
    float stunTimer = 0.0f;
    float stunShakePhase = 0.0f;
    float electrocuteTimer = 0.0f;
    float electrocutePhase = 0.0f;
    float movementDisableTimer = 0.0f;

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
    void UpdateDialog(UpdateContext &uc, float verticalOffset = 1.4f);
    bool isKnockbackActive() const { return this->knockbackTimer > 0.0f; }
    bool isStunned() const { return this->stunTimer > 0.0f; }
    float computeSupportHeightForRotation(const Quaternion &rotation) const;
    void snapToGroundWithRotation(const Quaternion &rotation);
    bool updateStun(UpdateContext &uc, float deltaSeconds);
    bool updateElectrocute(float deltaSeconds);
    void tickStatusTimers(float deltaSeconds);

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
        // healthDialog will be created by enemy implementations (cpp) where type is complete
    }
    
    Enemy(int customHealth)
    {
        position = {0};
        velocity = {0};
        direction = {0};
        grounded = true;
        health = customHealth;
        
        // Initialize animation state
        runTimer = 0.0f;
        runLerp = 0.0f;
        facingDirection = {0.0f, 0.0f, 1.0f};
    }

    virtual ~Enemy();

    // Updates the enemy's body (movement, jumping, etc.)
    void UpdateBody(UpdateContext& uc) override;
    bool damage(DamageResult &dResult);
    void applyKnockback(const Vector3 &pushVelocity, float durationSeconds, float lift = 0.0f);
    void applyStun(float durationSeconds);
    float getStunTime() const { return this->stunTimer; }
    // Identify this entity as an enemy for filtered queries
    EntityCategory category() const override;
    Vector3 getFacingDirection() const { return this->facingDirection; }
    virtual void gatherObjects(std::vector<Object *> &out) const;
    int getHealth() const { return this->health; }
    int getMaxHealth() const { return this->maxHealth; }
    void setMaxHealth(int newMaxHealth) { this->maxHealth = newMaxHealth; }
    void Heal(int amount)
    {
        this->health += amount;
        if (this->health > this->maxHealth)
            this->health = this->maxHealth;
    }
    TileType getTileType() const { return this->tileType; }
    void setTileType(TileType type) { this->tileType = type; }
    void disableVoluntaryMovement(float durationSeconds) { this->movementDisableTimer = fmaxf(this->movementDisableTimer, durationSeconds); }
    bool isMovementDisabled() const { return this->movementDisableTimer > 0.0f; }
    void applyElectrocute(float durationSeconds) { this->electrocuteTimer = fmaxf(this->electrocuteTimer, durationSeconds); this->electrocutePhase = 0.0f; }
    float getHealthPercent() const
    {
        if (this->maxHealth <= 0)
            return 0.0f;
        float percent = static_cast<float>(this->health) / static_cast<float>(this->maxHealth);
        if (percent < 0.0f)
            percent = 0.0f;
        if (percent > 1.0f)
            percent = 1.0f;
        return percent;
    }

    DialogBox *getHealthDialog() { return this->healthDialog; }
    
    // Virtual draw method for custom enemy visuals
    virtual void Draw() const;
};

// Minion: small, fast, low-health enemy used by Summoner
class MinionEnemy : public Enemy
{
private:
    enum class AttackState
    {
        Approaching,
        Launching,
        Cooldown
    };
    
    AttackState state = AttackState::Approaching;
    float attackCooldown = 0.0f;
    float cooldownDuration = 1.5f;
    float attackRange = 3.5f;
    float launchSpeed = 25.0f;
    float launchUpwardVelocity = 10.0f;
    float attackDamage = 15.0f;
    bool appliedDamage = false;
    
public:
    MinionEnemy() : Enemy(30) { this->setMaxHealth(30); this->setTileType(TileType::DOT_3); }
    void UpdateBody(UpdateContext &uc) override;
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
    ChargingEnemy() : Enemy(500) 
    { 
        this->setMaxHealth(500);  // Tank: 500 HP
        this->setTileType(TileType::CHARACTER_9); // Tank uses Character tiles
    }
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
    Texture2D sunTexture{};       // Texture for bullets
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
    ShooterEnemy();  // Constructor sets 250 HP (Sniper)
    ~ShooterEnemy();
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
    Vector3 spawnPosition = {0.0f, 0.0f, 0.0f};
    
    // Damage visual feedback
    float damageFlashTimer = 0.0f;
    float damageFlashDuration = 0.3f;
    int lastDamageAmount = 0;
    float damageNumberTimer = 0.0f;
    float damageNumberDuration = 1.5f;
    float damageNumberY = 0.0f;  // Y offset for floating animation

    // New: Struct to hold player input state
    

    // New: Private method for core player movement logic
    void applyPlayerMovement(UpdateContext& uc);
    float getColliderHalfHeight() const { return this->colliderHeight * 0.5f; }

public:
    Inventory hand;
    // Default constructor initializes the player with default values
    Me()
    {
        this->hand.CreatePlayerHand();
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
    void addCameraPitchKick(float magnitude, float durationSeconds);
    void applyKnockback(const Vector3 &pushVelocity, float durationSeconds, float lift = 0.0f);
    bool damage(DamageResult &dResult);
    int getHealth() const { return this->health; }
    void applyShootSlow(float slowFactor, float durationSeconds);
    float getMovementMultiplier() const;
    void respawn(const Vector3 &spawnPosition);
    void setSpawnPosition(const Vector3 &pos) { this->spawnPosition = pos; }
    Vector3 getSpawnPosition() const { return this->spawnPosition; }
    
    // Damage visual feedback getters
    float getDamageFlashAlpha() const { return Clamp(this->damageFlashTimer / this->damageFlashDuration, 0.0f, 1.0f); }
    bool hasDamageNumber() const { return this->damageNumberTimer > 0.0f; }
    int getLastDamageAmount() const { return this->lastDamageAmount; }
    float getDamageNumberAlpha() const { return 1.0f - (this->damageNumberTimer / this->damageNumberDuration); }
    float getDamageNumberY() const { return this->damageNumberY; }

    // Getter for the player's camera
    const Camera &getCamera() { return this->camera.getCamera(); }
    const Camera &getCamera() const { return this->camera.getCamera(); }

    // Getter for the player's look rotation (used for aiming and movement direction)
    Vector2 &getLookRotation() { return this->camera.lookRotation; }
    const Vector2 &getLookRotation() const { return this->camera.lookRotation; }
    // Identify this entity as the player
    EntityCategory category() const override;
};

// Summoner: periodically spawns MinionEnemy groups and tries to keep distance
class SummonerEnemy : public Enemy
{
private:
    enum class SummonState
    {
        Idle,       // Normal behavior, countdown to summon
        Ascending,  // Jumping upward in spiral
        Descending, // Falling with twirl, about to summon
        Summoning   // Brief moment at peak spawn, visual flash
    };
    
    SummonState summonState = SummonState::Idle;
    float spawnTimer = 0.0f;
    float spawnInterval = 9.0f; // Seconds between summon cycles
    int groupSize = 5; // Always spawn 5 minions
    float retreatDistance = 20.0f; // Retreat if player closer
    std::vector<MinionEnemy*> ownedMinions; // Track spawned minions for cleanup
    
    // Animation timing
    float animationTimer = 0.0f;
    float ascendDuration = 2.0f;      // Time to jump and spiral up
    float descendDuration = 0.3f;     // Time to spiral down and land
    float summonPeakDuration = 0.8f;  // Time at peak before spawning
    
    // Animation parameters
    float jumpHeight = 4.0f;           // How high summoner jumps
    float spiralRadius = 2.0f;         // Radius of spiral motion
    float twirls = 1.0f;               // Number of rotations during animation
    float startHeight = 0.0f;           // Height when animation starts
    float startAnimX = 0.0f;            // X position when animation starts
    float startAnimZ = 0.0f;            // Z position when animation starts
    
    // Visual effect counters
    Texture2D spiralParticleTexture = {0};
    float particleEmitTimer = 0.0f;
    float particleEmitRate = 20.0f;    // Particle per frame during animation
    
    void UpdateSummonAnimation(UpdateContext &uc, float delta);
    void EmitSummonParticles(const Vector3 &position, float intensity);
    void SpawnMinionGroup(UpdateContext &uc);
    void CleanupMinions(UpdateContext &uc);

public:
    SummonerEnemy() : Enemy(200) { this->setMaxHealth(200); this->setTileType(TileType::DOT_7); }
    ~SummonerEnemy();
    void UpdateBody(UpdateContext &uc) override;
    void OnDeath(UpdateContext &uc);
    void Draw() const override;
};

// Support: heals and buffs nearby allies
class SupportEnemy : public Enemy
{
private:
    enum class SupportMode
    {
        Normal,  // Hide behind other enemies, evade player
        Buff,    // Move to ally, charge up, apply buff
        Heal     // Move to ally, charge up, apply heal
    };

    SupportMode mode = SupportMode::Normal;
    
    // Mode parameters
    float normalSearchRadius = 30.0f;  // Find nearby allies to hide behind
    float normalHideDistance = 10.0f;   // Distance behind ally to stand
    float actionSearchRadius = 15.0f;  // Find targets for buff/heal within this radius
    float actionStandDistance = 8.0f;  // Move within this distance to target
    float actionChargeTime = 3.0f;     // Time to stand and charge before applying action (3s per request)
    float actionCooldown = 15.0f;       // Cooldown after buff/heal
    float retreatDistance = 25.0f;     // Retreat if player closer than this
    
    // Healing parameters
    float healingRate = 20.0f;         // HP per second when healing
    float healingThreshold = 0.4f;     // Only heal allies below 40% HP
    
    // Buff parameters
    float speedBuffAmount = 0.3f;      // 30% speed increase
    float buffDuration = 5.0f;         // Buff lasts 5 seconds
    
    // State tracking
    Enemy* targetAlly = nullptr;       // Current heal/buff target (or hide-behind ally)
    float actionTimer = 0.0f;          // Timer for charging before action
    float actionCooldownTimer = 0.0f;  // Countdown until next action possible
    
    // Particle emission timers
    float chargeParticleTimer = 0.0f;
    
    // Helper methods
    Enemy* FindAllyToHideBehind(UpdateContext &uc);
    Enemy* FindBestTarget(UpdateContext &uc, bool forHealing);
    Vector3 CalculateHidePosition(UpdateContext &uc, Enemy* allyToHideBehind);
    void UpdateNormalMode(UpdateContext &uc, const Vector3 &toPlayer);
    void UpdateBuffMode(UpdateContext &uc);
    void UpdateHealMode(UpdateContext &uc);
    void DrawGlowEffect(const Vector3 &pos, Color color, float intensity) const;

public:
    SupportEnemy() : Enemy(250) { this->setMaxHealth(250); this->setTileType(TileType::CHARACTER_1); }
    void UpdateBody(UpdateContext &uc) override;
    void Draw() const override;
};

// Vanguard: Heavy dragoon with aggressive ground combo and aerial dive attacks
class VanguardEnemy : public Enemy
{
private:
    enum class VanguardState
    {
        Chasing,
        GroundComboStab,
        GroundComboSlash,
        AerialAscend,
        AerialHover,
        AerialDive,
        AerialLanding
    };

    VanguardState state = VanguardState::Chasing;
    float stateTimer = 0.0f;

    // Ground combo parameters (Piston Thrust + Crescent Sweep)
    float comboAttackRange = 3.5f;       // Trigger range for combo
    
    // Stage 1: Piston Thrust (Linear Damage)
    float stabWindupTime = 1.0f;         // Telegraph time
    float stabActiveTime = 0.15f;        // Strike time
    float stabRecoveryTime = 0.3f;       // Recovery before sweep
    float stabWeaponLength = 6.0f;       // Long range stab
    float stabDamage = 20.0f;
    float stabLungeForce = 15.0f;        // Forward slide during stab
    
    // Stage 2: Crescent Sweep (Area Damage)
    float slashWindupTime = 1.2f;        // Turn & drag time
    float slashActiveTime = 0.35f;       // Cleave time (increased for longer dash)
    float slashRecoveryTime = 1.0f;      // Long recovery (punish window)
    float slashDamage = 25.0f;
    float slashArcDegrees = 180.0f;      // Wide semi-circle
    float slashRange = 4.0f;             // Shorter than stab
    
    int comboStage = 0;                  // 0 = none, 1 = stab, 2 = slash
    bool comboHitPlayer = false;         // Track if hit landed
    Vector3 stabDirection = {0, 0, 1};   // Store stab direction

    // Aerial dive parameters (focus purely on dive, no teleport)
    float diveCooldownDuration = 6.0f;
    float diveCooldownTimer = 0.0f;
    float diveChancePerFrame = 0.01f;       // Chance each frame when in range
    
    // AI decision cooldown
    float decisionCooldownDuration = 1.0f;  // Make decisions every 1 second
    float decisionCooldownTimer = 0.0f;
    
    // Aerial Dive Attack: Quick ascent to hover, then aggressive dash dive with landing shockwave
    float diveAscendTime = 0.4f;            // Quick ascent phase (0.4s)
    float diveHangTime = 1.5f;              // 1.5 seconds hover at peak, staring at player camera
    float diveAscendInitialVelocity = 35.0f; // Upward velocity during ascent
    float diveGravityDuringAscent = 56.0f;   // Gravity applied during hover/descent
    float diveDamage = 45.0f;               // Damage on direct hit
    float diveLandingRecoveryTime = 2.0f;   // Recovery time after landing
    float diveImpactSquashTime = 0.15f;     // Duration of squash effect on impact
    Vector3 diveTargetPos = {0.0f, 0.0f, 0.0f};
    float diveInitialSpeed = 15.0f;         // Initial horizontal speed during dive start
    float diveAcceleration = 300.0f;        // Aggressive acceleration during descent
    float diveMaxSpeed = 150.0f;            // Very high terminal velocity
    float diveCurrentSpeed = 0.0f;          // Track current dive speed
    
    // Shockwave: Expanding ring on dive impact that damages grounded players (can be jumped over)
    float shockwaveRadius = 0.0f;           // Current radius of expanding shockwave
    float shockwaveMaxRadius = 22.0f;       // Max radius before shockwave stops
    float shockwaveExpandSpeed = 18.0f;     // Units per second
    Vector3 shockwaveCenter = {0.0f, 0.0f, 0.0f}; // Center of shockwave
    bool shockwaveActive = false;           // Is shockwave currently active
    float shockwaveDamage = 22.0f;          // Damage dealt by shockwave
    bool shockwaveHitPlayer = false;        // Track if we hit player with this shockwave
    
    // Animation & Visual State
    Vector3 visualScale = {1.0f, 1.0f, 1.0f}; // For squash & stretch on landing
    float rotationTowardsPlayer = 0.0f;       // Rotation blending during aerial states

    // Movement
    float chaseSpeed = 6.0f;                  // Speed while chasing player
    
    // Spear Weapon Visual: Animated based on attack state (Piston Thrust stab or Crescent Sweep slash)
    // Model is shared between all Vanguard instances and animated in world space
    static Model sharedSpearModel;
    static bool spearModelLoaded;
    Vector3 spearOffset = {1.2f, 0.0f, 0.0f}; // Hold at side by default
    Vector3 spearRotationOffset = {0, 0, 0};  // Rotation adjustment for pointing at camera
    float spearScale = 0.0075f;                // Model scale multiplier
    float spearThrustAmount = 0.0f;           // 0-1 for stab forward extension
    float spearRetractAmount = 0.0f;          // 0-1 for stab pull-back (windup)
    float spearSwingAngle = 0.0f;             // Angle for slash swing (degrees from center)
    float spearSwingStartAngle = -90.0f;      // Start angle for slash arc (left side)
    float spearLingerTimer = 0.0f;            // Hold spear extended after attack
    float spearLingerDuration = 0.4f;         // Duration to hold spear visible
    mutable Vector3 smoothedSpearPos = {0.0f, 0.0f, 0.0f};  // Smoothed position (jitter reduction)
    mutable float smoothedYRotation = 0.0f;                 // Smoothed rotation (jitter reduction)
    
    // Cached camera info: Updated each frame to point weapon at camera
    Vector3 cachedCameraPos = {0.0f, 0.0f, 0.0f};
    float cachedCameraYawDeg = 0.0f;
    float cachedCameraPitchDeg = 0.0f;

    // Helper methods
    Vector3 CalculateBackstabPosition(UpdateContext &uc);
    void HandleGroundCombo(UpdateContext &uc);  // Two-stage combo: Piston Thrust stab then Crescent Sweep slash
    void HandleAerialDive(UpdateContext &uc);   // Ascend -> Hover -> Dive with shockwave
    bool CheckStabHit(UpdateContext &uc);       // Collision check for stab attack
    bool CheckSlashHit(UpdateContext &uc);      // Collision check for slash attack
    void DecideAction(UpdateContext &uc, float distanceToPlayer);  // AI decision making based on distance

public:
    VanguardEnemy() : Enemy(180) { this->setMaxHealth(180); this->setTileType(TileType::DRAGON_RED); }
    static void LoadSharedResources();  // Load spear model once at game start
    static void UnloadSharedResources(); // Cleanup on game end
    void UpdateBody(UpdateContext &uc) override;
    void Draw() const override;
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
    TileType type;
    float damage = 10.0f; // Damage dealt by this projectile
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
    Projectile(Vector3 pos, Vector3 vel, Vector3 d, bool g, Object o1, float fric, float aird, TileType _type)
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