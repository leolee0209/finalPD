#include "me.hpp"
#include "scene.hpp"
#include "constant.hpp"
#include "raymath.h"
#include <iostream>

// Updates the player's state based on user input and physics
void Me::applyPlayerMovement(UpdateContext &uc)
{
    // Get player look direction
    float rot = this->camera.lookRotation.x;

    // Normalize input vector to prevent faster diagonal movement
    Vector2 input = {(float)uc.playerInput.side, (float)-uc.playerInput.forward};
    if ((uc.playerInput.side != 0) && (uc.playerInput.forward != 0))
        input = Vector2Normalize(input);

    float delta = GetFrameTime();

    bool knockedBack = this->knockbackTimer > 0.0f;
    if (knockedBack)
    {
        this->knockbackTimer = fmaxf(0.0f, this->knockbackTimer - delta);
        knockedBack = this->knockbackTimer > 0.0f;
    }

    if (this->meleeWindupTimer > 0.0f)
    {
        this->meleeWindupTimer = fmaxf(0.0f, this->meleeWindupTimer - delta);
    }

    // Update shoot slow timer
    if (this->shootSlowTimer > 0.0f)
    {
        this->shootSlowTimer = fmaxf(0.0f, this->shootSlowTimer - delta);
    }

    bool lockMovement = this->meleeWindupTimer > 0.0f;
    if (knockedBack || lockMovement)
    {
        input = {0.0f, 0.0f};
        this->direction = Vector3Zero();
    }
    if (lockMovement)
    {
        this->velocity.x = Lerp(this->velocity.x, 0.0f, 30.0f * delta);
        this->velocity.z = Lerp(this->velocity.z, 0.0f, 30.0f * delta);
    }

    bool airborne = !this->grounded;
    if (airborne)
    {
        input = {0.0f, 0.0f};
    }

    // Handle jumping (gravity & vertical motion handled by ApplyPhysics)
    if (this->grounded && uc.playerInput.jumpPressed)
    {
        if (!lockMovement)
        {
            this->velocity.y = JUMP_FORCE;
            this->grounded = false;
        }
    }

    // Calculate the direction the player is facing
    Vector3 front = {sinf(rot), 0.f, cosf(rot)};
    Vector3 right = {cosf(-rot), 0.f, sinf(-rot)};

    // Calculate desired movement direction based on input
    Vector3 desiredDir = {
        input.x * right.x + input.y * front.x,
        0.0f,
        input.x * right.z + input.y * front.z,
    };
    // Smoothly interpolate to the desired direction
    this->direction = Vector3Lerp(this->direction, desiredDir, CONTROL * delta);

    // Use shared physics helper to handle gravity, friction/air-drag, accel,
    // position integration, collisions and floor handling.
    Entity::PhysicsParams params;
    params.useGravity = true;
    params.gravity = GRAVITY;
    params.decelGround = FRICTION;
    params.decelAir = AIR_DRAG;
    float baseSpeed = (uc.playerInput.crouchHold ? CROUCH_SPEED : MAX_SPEED);
    params.maxSpeed = baseSpeed * this->getMovementMultiplier();
    params.maxAccel = airborne ? 0.0f : MAX_ACCEL;
    params.floorY = this->getColliderHalfHeight();
    params.iterativeCollisionResolve = true;
    params.zeroThreshold = params.maxSpeed * 0.01f;

    Entity::ApplyPhysics(this, uc, params);
}

void Me::UpdateBody(UpdateContext &uc)
{
    this->applyPlayerMovement(uc);
    if (this->meleeSwingTimer > 0.0f)
    {
        float delta = GetFrameTime();
        this->meleeSwingTimer = fmaxf(0.0f, this->meleeSwingTimer - delta);
    }
    this->UpdateCamera(uc);
}

// Updates the camera's position and orientation based on player movement
void Me::UpdateCamera(UpdateContext &uc)
{
    this->camera.UpdateCamera(uc.playerInput.side, uc.playerInput.forward, uc.playerInput.crouchHold, this->position, this->getColliderHalfHeight(), this->grounded, this->getMeleeSwingAmount());
}

void Me::triggerMeleeSwing(float durationSeconds)
{
    if (durationSeconds > 0.0f)
        this->meleeSwingDuration = durationSeconds;
    this->meleeSwingTimer = this->meleeSwingDuration;
}

float Me::getMeleeSwingAmount() const
{
    if (this->meleeSwingDuration <= 0.0f)
        return 0.0f;
    return Clamp(this->meleeSwingTimer / this->meleeSwingDuration, 0.0f, 1.0f);
}

void Me::beginMeleeWindup(float durationSeconds)
{
    if (durationSeconds <= 0.0f)
        return;
    this->meleeWindupTimer = fmaxf(this->meleeWindupTimer, durationSeconds);
}

bool Me::isInMeleeWindup() const
{
    return this->meleeWindupTimer > 0.0f;
}

void Me::addCameraShake(float magnitude, float duration)
{
    this->camera.addShake(magnitude, duration);
}

void Me::addCameraFovKick(float magnitude, float durationSeconds)
{
    this->camera.addFovKick(magnitude, durationSeconds);
}

void Me::applyKnockback(const Vector3 &pushVelocity, float durationSeconds, float lift)
{
    this->velocity.x += pushVelocity.x;
    this->velocity.z += pushVelocity.z;
    if (lift > 0.0f)
    {
        this->velocity.y = fmaxf(this->velocity.y, lift);
    }
    this->grounded = false;
    this->knockbackTimer = fmaxf(this->knockbackTimer, durationSeconds);
}

bool Me::damage(DamageResult &dResult)
{
    int appliedDamage = (int)(dResult.damage);
    if (appliedDamage <= 0)
        appliedDamage = 1;
    this->health -= appliedDamage;
    if (this->health < 0)
        this->health = 0;
    float shakeMagnitude = Clamp((float)appliedDamage / 40.0f, 0.1f, 0.7f);
    this->addCameraShake(shakeMagnitude, 0.25f);
    return this->health > 0;
}

EntityCategory Me::category() const
{
    return ENTITY_PLAYER;
}

void Me::applyShootSlow(float slowFactor, float durationSeconds)
{
    if (durationSeconds <= 0.0f)
        return;
    this->shootSlowTimer = fmaxf(this->shootSlowTimer, durationSeconds);
    this->shootSlowFactor = slowFactor;
}

float Me::getMovementMultiplier() const
{
    if (this->shootSlowTimer > 0.0f)
        return this->shootSlowFactor;
    return 1.0f;
}

void Me::respawn(const Vector3 &spawnPosition)
{
    // Reset position
    this->position = spawnPosition;
    this->position.y = this->getColliderHalfHeight();
    this->o.pos = this->position;
    this->o.UpdateOBB();
    
    // Reset physics
    this->velocity = Vector3Zero();
    this->direction = Vector3Zero();
    this->grounded = true;
    
    // Reset health
    this->health = MAX_HEALTH_ME;
    
    // Reset timers
    this->knockbackTimer = 0.0f;
    this->meleeSwingTimer = 0.0f;
    this->meleeWindupTimer = 0.0f;
    this->shootSlowTimer = 0.0f;
    
    // Reset camera
    this->camera.resetShake();
    Vector3 cameraPos = this->position;
    cameraPos.y += this->getColliderHalfHeight();
    this->camera.setPosition(cameraPos);
}

// Updates the projectile's body (movement, gravity, etc.)
void Projectile::UpdateBody(UpdateContext &uc)
{
    // Use shared physics helper for the core physics/integration steps.
    Entity::PhysicsParams params;
    params.useGravity = true;
    params.gravity = GRAVITY;
    params.decelGround = this->friction;
    params.decelAir = this->airDrag;
    // projectile doesn't accelerate by direction input
    params.maxSpeed = 0.0f;
    params.maxAccel = 0.0f;
    params.floorY = 0.0f;
    params.iterativeCollisionResolve = false;
    params.zeroThreshold = 0.01f; // small threshold for zeroing hvel

    Entity::ApplyPhysics(this, uc, params);

    // Do projectile-specific collision handling (damage to enemies)
    auto results = Object::collided(this->o, uc.scene);
    for (auto &result : results)
    {
        if (result.with && result.with->category() == ENTITY_ENEMY)
        {
            Enemy *e = static_cast<Enemy *>(result.with);
            auto dResult = DamageResult(10, result);
            uc.scene->em.damage(e, dResult, uc);
        }
    }
}

EntityCategory Projectile::category() const
{
    return ENTITY_PROJECTILE;
}

void Entity::resolveCollision(Entity *e, UpdateContext &uc)
{
    for (int i = 0; i < 5; i++)
    {
        std::vector<CollisionResult> results = Object::collided(e->o, uc.scene);
        if (e != uc.player)
        {
            auto playerResult = Object::collided(e->o, uc.player->obj());
            if (playerResult.collided)
                results.push_back(playerResult);
        }
        if (results.size() == 0)
            break;
        for (auto &result : results)
        {
            if (result.with == e)
                continue;
            e->position = Vector3Add(e->position, Vector3Scale(result.normal, result.penetration));
            e->o.pos = e->position;
            e->o.UpdateOBB();

            float dot = Vector3DotProduct(e->velocity, result.normal);
            if (dot < 0)
            {
                e->velocity = Vector3Subtract(e->velocity, Vector3Scale(result.normal, dot));
            }
            // if (fabs(result.normal.y) > 0.7f && result.normal.y > 0)
            // {
            //     e->grounded = true;
            // }
        }
    }
}

void Entity::ApplyPhysics(Entity *e, UpdateContext &uc, const PhysicsParams &p)
{
    float delta = GetFrameTime();

    // 1. Gravity
    if (p.useGravity && !e->grounded)
    {
        e->velocity.y -= p.gravity * delta;
    }

    // 2. Apply friction or air drag to horizontal velocity
    float decel = (e->grounded ? p.decelGround : p.decelAir);
    Vector3 hvel = {e->velocity.x * decel, 0.0f, e->velocity.z * decel};

    // 3. Zero small horizontal velocity
    float threshold = (p.zeroThreshold > 0.0f) ? p.zeroThreshold : (p.maxSpeed > 0.0f ? p.maxSpeed * 0.01f : MAX_SPEED * 0.01f);
    if (Vector3Length(hvel) < threshold)
    {
        hvel = {0, 0, 0};
    }

    // 4. Optionally apply acceleration along entity direction
    if (p.maxAccel > 0.0f)
    {
        float speed = Vector3DotProduct(hvel, e->direction);
        float accel = Clamp(p.maxSpeed - speed, 0.f, p.maxAccel * delta);
        hvel.x += e->direction.x * accel;
        hvel.z += e->direction.z * accel;
    }

    e->velocity.x = hvel.x;
    e->velocity.z = hvel.z;

    // 5. Integrate position
    e->position.x += e->velocity.x * delta;
    e->position.y += e->velocity.y * delta;
    e->position.z += e->velocity.z * delta;

    // 6. Update object and OBB
    e->o.pos = e->position;
    e->o.UpdateOBB();

    // 7. Optional iterative collision resolution
    if (p.iterativeCollisionResolve)
    {
        Entity::resolveCollision(e, uc);
    }

    // 8. Floor collision (using provided floorY)
    if (e->position.y <= p.floorY)
    {
        e->position.y = p.floorY;
        e->velocity.y = 0.0f;
        e->grounded = true;
    }
    else if (e->grounded && e->velocity.y < p.floorY + 0.01f)
    {
        e->grounded = false;
    }

    // 9. Final OBB update after any positional corrections
    e->o.pos = e->position;
    e->o.UpdateOBB();
}
