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
    if (!this->grounded)
    {
        this->velocity.y -= GRAVITY * delta;
    }

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

    // 3. Set enemy's desired movement direction (Physics uses snappy input)
    this->direction = directionToPlayer;

    // 4. Apply friction or air drag to horizontal velocity
    float decel = (this->grounded ? FRICTION : AIR_DRAG);
    Vector3 hvel = {this->velocity.x * decel, 0.0f, this->velocity.z * decel};

    // 5. If horizontal velocity is very low, set it to zero
    float hvelLength = Vector3Length(hvel);
    if (hvelLength < (MAX_SPEED * 0.01f))
    {
        hvel = {0};
    }

    float speed = Vector3DotProduct(hvel, this->direction);

    // 6. Clamp acceleration to avoid exceeding max speed
    float maxEnemySpeed = 3.0f;
    float accel = Clamp(maxEnemySpeed - speed, 0.f, MAX_ACCEL * delta);

    // 7. Apply acceleration to horizontal velocity
    hvel.x += this->direction.x * accel;
    hvel.z += this->direction.z * accel;

    this->velocity.x = hvel.x;
    this->velocity.z = hvel.z;

    // 8. Update position based on velocity
    this->position.x += this->velocity.x * delta;
    this->position.y += this->velocity.y * delta;
    this->position.z += this->velocity.z * delta;

    // 9. Update the internal Object's position and OBB
    this->o.pos = this->position;
    this->o.UpdateOBB();

    // 10. Iterative collision resolution
    Entity::resolveCollision(this, uc);

    // 11. Floor collision
    if (this->position.y <= floory)
    {
        this->position.y = floory;
        this->velocity.y = 0;
        this->grounded = true;
    }
    else if (this->grounded && this->velocity.y < floory + 0.01f)
    {
        this->grounded = false;
    }

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
    float targetRunLerp = (horizontalSpeed > 0.1f && this->grounded) ? 1.0f : 0.0f;
    this->runLerp = Lerp(this->runLerp, targetRunLerp, 10.0f * delta);

    if (this->runLerp > 0.01f)
    {
        this->runTimer += delta * 15.0f;
    }

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
    this->o.rotate(rightDir, leanAngle);

    // 13. Final Update OBB
    this->o.UpdateOBB();
}

bool Enemy::damage(DamageResult &dResult)
{
    this->health -= dResult.damage;
    return this->health > 0;
}
