#include "me.hpp"       // Contains Enemy class definition
#include "scene.hpp"    // For Scene class and its methods
#include "raymath.h"    // For Vector3 operations
#include "constant.hpp" // For constants like GRAVITY, FRICTION, AIR_DRAG, MAX_SPEED, MAX_ACCEL
#include <iostream>
#include <cmath> // For sinf, cosf

// Implement Enemy::UpdateBody
void Enemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    float floory = this->o.size.y / 2;

    // Reset visual object position to physical position before physics calculations
    this->o.pos = this->position;

    // 1. Apply gravity if not grounded
    // 2. Calculate direction to player
    Vector3 directionToPlayer = Vector3Subtract(uc.player->pos(), this->position);
    directionToPlayer.y = 0; // Ignore Y-axis for horizontal movement direction

    if (Vector3LengthSqr(directionToPlayer) > 0.001f)
    {
        directionToPlayer = Vector3Normalize(directionToPlayer);
    }
    else
    {
        directionToPlayer = {0};
    }

    bool knockedBack = this->knockbackTimer > 0.0f;
    if (knockedBack)
    {
        this->knockbackTimer -= delta;
        if (this->knockbackTimer < 0.0f)
            this->knockbackTimer = 0.0f;
    }

    // 3. Set enemy's desired movement direction (Physics uses snappy input)
    this->direction = knockedBack ? Vector3Zero() : directionToPlayer;

    // 4. Use shared physics helper with enemy-specific tuning
    Entity::PhysicsParams params;
    params.useGravity = true;
    params.gravity = GRAVITY;
    params.decelGround = FRICTION;
    params.decelAir = AIR_DRAG;
    float maxEnemySpeed = 3.0f;
    params.maxSpeed = maxEnemySpeed;
    params.maxAccel = MAX_ACCEL;
    params.floorY = floory;
    params.iterativeCollisionResolve = true;
    params.zeroThreshold = maxEnemySpeed * 0.01f;

    Entity::ApplyPhysics(this, uc, params);

    // --- SMOOTH ROTATION LOGIC ---

    // Smoothly update the visual facing direction towards the target
    if (Vector3LengthSqr(directionToPlayer) > 0.001f)
    {
        float turnSpeed = 4.0f; // Lower value = slower, smoother turn
        this->facingDirection = Vector3Lerp(this->facingDirection, directionToPlayer, turnSpeed * delta);
        this->facingDirection = Vector3Normalize(this->facingDirection);
    }
    // Note: We don't change facingDirection if directionToPlayer is 0 (enemy stops looking around)

    // 12. Make the enemy rotate to face the smoothed direction
    this->o.setRotationFromForward(this->facingDirection);

    // --- Vivid Movement Animation ---

    // Calculate horizontal speed
    float horizontalSpeed = Vector3Length({this->velocity.x, 0, this->velocity.z});

    // Update Animation State
    float targetRunLerp = (horizontalSpeed > 0.1f && this->grounded && !knockedBack) ? 1.0f : 0.0f;
    this->runLerp = Lerp(this->runLerp, targetRunLerp, 10.0f * delta);

    if (this->runLerp > 0.01f)
    {
        this->runTimer += delta * 15.0f;
    }
    else
    {
        this->runTimer = Lerp(this->runTimer, 0.0f, 5.0f * delta);
    }

    // Gradually recover the visual 'hit tilt' over time. A higher
    // damping value here makes the tilt snap back faster; lower values
    // produce a slower, more pronounced recovery.
    this->hitTilt = Lerp(this->hitTilt, 0.0f, 6.0f * delta);

    // 1. Apply Bobbing
    float bobY = fabsf(cosf(this->runTimer)) * 0.2f * this->runLerp;
    this->o.pos.y += bobY;

    // 2. Apply Sway
    float swayAngle = sinf(this->runTimer) * 10.0f * this->runLerp;

    // Use smoothed facing direction for sway/lean axis
    Vector3 forwardDir = this->facingDirection;
    if (Vector3LengthSqr(forwardDir) < 0.001f)
        forwardDir = {0, 0, 1};

    this->o.rotate(forwardDir, swayAngle);

    // 3. Apply Lean
    float leanAngle = horizontalSpeed * 2.0f;
    Vector3 rightDir = Vector3CrossProduct(forwardDir, {0, 1, 0});
    // Apply a base movement lean (sway) based on horizontal speed.
    // Additional hit-based tilt is applied below using `hitTilt`.
    this->o.rotate(rightDir, leanAngle);

    if (this->hitTilt > 0.01f)
    {
        // When hitTilt > 0 the enemy will lean/tilt in the opposite
        // direction to visually indicate being struck. The multiplier
        // controls how dramatic the effect is (degrees per unit hitTilt).
        this->o.rotate(rightDir, -this->hitTilt * 40.0f);
    }

    // 13. Final Update OBB
    this->o.UpdateOBB();
}

bool Enemy::damage(DamageResult &dResult)
{
    this->health -= dResult.damage;
    return this->health > 0;
}

EntityCategory Enemy::category() const
{
    return ENTITY_ENEMY;
}

void Enemy::applyKnockback(const Vector3 &pushVelocity, float durationSeconds, float lift)
{
    this->velocity.x += pushVelocity.x;
    this->velocity.z += pushVelocity.z;
    if (lift > 0.0f)
    {
        this->velocity.y = fmaxf(this->velocity.y, lift);
    }
    this->grounded = false;
    this->knockbackTimer = durationSeconds;
    // Set hitTilt proportional to the horizontal knockback strength.
    // Using the horizontal magnitude gives a consistent visual feel for
    // pushes regardless of vertical lift. The divisor and clamp bound
    // the tilt so a very strong push does not produce unrealistic rotation.
    Vector3 horiz = {pushVelocity.x, 0.0f, pushVelocity.z};
    float strength = Vector3Length(horiz);
    // Map strength to a 0..1.5 range so strong hits feel dramatic but don't flip
    this->hitTilt = Clamp(strength / 30.0f, 0.0f, 1.5f);
}
