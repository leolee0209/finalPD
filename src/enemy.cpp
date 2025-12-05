#include "me.hpp"       // Contains Enemy class definition
#include "dialogBox.hpp"
#include "scene.hpp"    // For Scene class and its methods
#include "raymath.h"    // For Vector3 operations
#include "constant.hpp" // For constants like GRAVITY, FRICTION, AIR_DRAG, MAX_SPEED, MAX_ACCEL
#include <iostream>
#include <cstdio>
#include <cmath> // For sinf, cosf
#include <algorithm>

// ---------------------------- MinionEnemy ----------------------------
void MinionEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->tickStatusTimers(delta);
    bool isStunned = this->updateStun(uc);

    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    float dist = Vector3Length(toPlayer);

    Vector3 desiredDir = Vector3Zero();
    Enemy::MovementSettings settings;
    settings.lockToGround = true;
    settings.maxSpeed = 7.5f;
    settings.maxAccel = MAX_ACCEL * 1.2f;
    settings.decelGround = FRICTION * 1.1f;
    settings.decelAir = AIR_DRAG;
    settings.facingHint = toPlayer;

    if (!this->isKnockbackActive() && !isStunned && !this->isMovementDisabled())
    {
        switch (this->state)
        {
        case AttackState::Approaching:
            if (dist > this->attackRange)
            {
                // Chase player
                desiredDir = Vector3Normalize(toPlayer);
            }
            else if (this->isGrounded())
            {
                // Launch attack when in range and grounded
                this->state = AttackState::Launching;
                Vector3 launchVel = Vector3Scale(Vector3Normalize(toPlayer), this->launchSpeed);
                launchVel.y = this->launchUpwardVelocity;
                this->setVelocity(launchVel);
                this->appliedDamage = false;
                desiredDir = Vector3Zero();
            }
            break;

        case AttackState::Launching:
            // In air - check for collision with player
            if (!this->appliedDamage)
            {
                CollisionResult hitResult = GetCollisionOBBvsOBB(&this->obj().obb, &uc.player->obj().obb);
                if (hitResult.collided)
                {
                    DamageResult damage(this->attackDamage, hitResult);
                    uc.player->damage(damage);
                    Vector3 knockDir = Vector3Normalize(toPlayer);
                    if (Vector3LengthSqr(knockDir) < 0.0001f)
                        knockDir = {0.0f, 0.0f, 1.0f};
                    uc.player->applyKnockback(Vector3Scale(knockDir, 8.0f), 0.3f, 0.0f);
                    this->appliedDamage = true;
                }
            }
            
            // Return to cooldown when grounded
            if (this->isGrounded())
            {
                this->state = AttackState::Cooldown;
                this->attackCooldown = this->cooldownDuration;
            }
            desiredDir = Vector3Zero();
            break;

        case AttackState::Cooldown:
            // Wait before next attack
            this->attackCooldown -= delta;
            if (this->attackCooldown <= 0.0f)
            {
                this->state = AttackState::Approaching;
            }
            // Stand still during cooldown
            desiredDir = Vector3Zero();
            break;
        }
    }

    if (this->isMovementDisabled() || isStunned)
    {
        desiredDir = Vector3Zero();
        settings.maxSpeed = 0.0f;
        settings.maxAccel = 0.0f;
    }

    this->UpdateCommonBehavior(uc, desiredDir, delta, settings);
    this->updateElectrocute(delta);
    this->UpdateDialog(uc);
}

void Enemy::UpdateCommonBehavior(UpdateContext &uc, const Vector3 &desiredDirection, float deltaSeconds, const MovementSettings &settings)
{
    float floory = this->computeSupportHeightForRotation(this->o.getRotation());

    this->o.pos = this->position;

    Vector3 moveDir = desiredDirection;
    if (Vector3LengthSqr(moveDir) > 0.001f)
    {
        moveDir = Vector3Normalize(moveDir);
    }
    else
    {
        moveDir = Vector3Zero();
    }

    if (this->isMovementDisabled())
    {
        moveDir = Vector3Zero();
    }

    if (this->knockbackTimer > 0.0f)
    {
        this->knockbackTimer -= deltaSeconds;
        if (this->knockbackTimer < 0.0f)
            this->knockbackTimer = 0.0f;
    }
    bool knockedBack = this->knockbackTimer > 0.0f;

    this->direction = knockedBack ? Vector3Zero() : moveDir;

    if (settings.overrideHorizontalVelocity && !knockedBack)
    {
        this->velocity.x = settings.forcedHorizontalVelocity.x;
        this->velocity.z = settings.forcedHorizontalVelocity.z;
    }

    Entity::PhysicsParams params;
    params.useGravity = true;
    params.gravity = GRAVITY;
    params.decelGround = settings.decelGround;
    params.decelAir = settings.decelAir;
    params.maxSpeed = settings.maxSpeed;
    params.maxAccel = settings.maxAccel;
    params.floorY = floory;
    params.iterativeCollisionResolve = true;
    if (settings.zeroThreshold >= 0.0f)
    {
        params.zeroThreshold = settings.zeroThreshold;
    }
    else
    {
        params.zeroThreshold = (settings.maxSpeed > 0.0f) ? settings.maxSpeed * 0.01f : MAX_SPEED * 0.01f;
    }

    Entity::ApplyPhysics(this, uc, params);

    if (settings.lockToGround && !knockedBack)
    {
        // Only snap if we're essentially on the floor; allow airborne knockups
        if (this->position.y <= floory + 0.05f)
        {
            this->position.y = floory;
            this->velocity.y = 0.0f;
            this->grounded = true;
            this->o.pos = this->position;
        }
        else
        {
            this->grounded = false;
        }
    }

    Vector3 facingTarget = settings.facingHint;
    if (Vector3LengthSqr(facingTarget) < 0.001f)
    {
        facingTarget = moveDir;
    }

    if (Vector3LengthSqr(facingTarget) > 0.001f)
    {
        float turnSpeed = 4.0f;
        Vector3 blended = Vector3Lerp(this->facingDirection, Vector3Normalize(facingTarget), turnSpeed * deltaSeconds);
        if (Vector3LengthSqr(blended) > 0.001f)
        {
            this->facingDirection = Vector3Normalize(blended);
        }
    }

    this->o.setRotationFromForward(this->facingDirection);

    float horizontalSpeed = Vector3Length({this->velocity.x, 0, this->velocity.z});

    float targetRunLerp = (horizontalSpeed > 0.1f && this->grounded && !knockedBack) ? 1.0f : 0.0f;
    this->runLerp = Lerp(this->runLerp, targetRunLerp, 10.0f * deltaSeconds);

    if (this->runLerp > 0.01f)
    {
        this->runTimer += deltaSeconds * 15.0f;
    }
    else
    {
        this->runTimer = Lerp(this->runTimer, 0.0f, 5.0f * deltaSeconds);
    }

    this->hitTilt = Lerp(this->hitTilt, 0.0f, 6.0f * deltaSeconds);

    float swayAngle = 0.0f;
    Vector3 forwardDir = this->getFacingDirection();
    if (Vector3LengthSqr(forwardDir) < 0.001f)
        forwardDir = {0, 0, 1};

    Vector3 rightDir = Vector3CrossProduct(forwardDir, {0, 1, 0});

    if (settings.enableBobAndSway)
    {
        float bobY = fabsf(cosf(this->runTimer)) * 0.2f * this->runLerp;
        this->o.pos.y += bobY;
        swayAngle = sinf(this->runTimer) * 10.0f * this->runLerp;
        if (fabsf(swayAngle) > 0.001f)
        {
            this->o.rotate(forwardDir, swayAngle);
        }
    }

    if (settings.enableLean)
    {
        float leanAngle = horizontalSpeed * settings.leanScale;
        if (settings.maxLeanAngle > 0.0f)
        {
            leanAngle = Clamp(leanAngle, -settings.maxLeanAngle, settings.maxLeanAngle);
        }
        if (fabsf(leanAngle) > 0.001f)
        {
            this->o.rotate(rightDir, leanAngle);
        }
    }

    if (this->hitTilt > 0.01f)
    {
        this->o.rotate(rightDir, -this->hitTilt * 40.0f);
    }

    if (settings.lockToGround && !knockedBack)
    {
        if (this->position.y <= this->computeSupportHeightForRotation(this->o.getRotation()) + 0.05f)
        {
            this->snapToGroundWithRotation(this->o.getRotation());
        }
    }

    this->o.UpdateOBB();
}

Enemy::~Enemy()
{
    delete this->healthDialog;
    this->healthDialog = nullptr;
}

float Enemy::computeSupportHeightForRotation(const Quaternion &rotation) const
{
    Vector3 halfSize = Vector3Scale(this->o.size, 0.5f);
    Vector3 worldRight = Vector3RotateByQuaternion({1.0f, 0.0f, 0.0f}, rotation);
    Vector3 worldUp = Vector3RotateByQuaternion({0.0f, 1.0f, 0.0f}, rotation);
    Vector3 worldForward = Vector3RotateByQuaternion({0.0f, 0.0f, 1.0f}, rotation);

    float supportHeight = halfSize.x * fabsf(worldRight.y) +
                          halfSize.y * fabsf(worldUp.y) +
                          halfSize.z * fabsf(worldForward.y);

    if (supportHeight < 0.0f)
        supportHeight = 0.0f;

    return supportHeight;
}

void Enemy::snapToGroundWithRotation(const Quaternion &rotation)
{
    float supportHeight = this->computeSupportHeightForRotation(rotation);
    this->position.y = supportHeight;
    this->o.pos.y = supportHeight;
    this->velocity.y = 0.0f;
    this->grounded = true;
}

// Implement Enemy::UpdateBody
void Enemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->tickStatusTimers(delta);
    bool isStunned = this->updateStun(uc);

    Vector3 directionToPlayer = Vector3Subtract(uc.player->pos(), this->position);
    directionToPlayer.y = 0.0f;

    MovementSettings settings;
    settings.maxSpeed = 3.0f;
    settings.facingHint = directionToPlayer;

    if (this->isMovementDisabled() || isStunned)
    {
        directionToPlayer = Vector3Zero();
        settings.maxSpeed = 0.0f;
        settings.maxAccel = 0.0f;
    }

    this->UpdateCommonBehavior(uc, directionToPlayer, delta, settings);
    this->updateElectrocute(delta);
    // Update health dialog owned by this enemy (position/text/orientation)
    this->UpdateDialog(uc);
}

void ChargingEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->tickStatusTimers(delta);
    bool isStunned = this->updateStun(uc);

    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    float distanceToPlayer = Vector3Length(toPlayer);

    Vector3 desiredDirection = Vector3Zero();
    float targetSpeed = this->approachSpeed;
    float targetPoseDeg = 0.0f;
    bool usesStateTimer = false;
    bool timerWaitsForPose = false;

    if (!isStunned && !this->isMovementDisabled())
    {
        switch (this->state)
        {
        case ChargeState::Approaching:
            if (distanceToPlayer > 0.1f && distanceToPlayer <= this->stopDistance)
            {
                this->state = ChargeState::Windup;
                this->stateTimer = this->windupDuration;
                this->poseAngularVelocityDegPerSec = 0.0f;
                if (Vector3LengthSqr(toPlayer) > 0.001f)
                {
                    this->chargeDirection = Vector3Normalize(toPlayer);
                }
                desiredDirection = Vector3Zero();
                targetSpeed = 0.0f;
                targetPoseDeg = -90.0f;
                usesStateTimer = true;
                timerWaitsForPose = true;
            }
            else
            {
                desiredDirection = toPlayer;
                targetSpeed = this->approachSpeed;
                targetPoseDeg = 0.0f;
            }
            break;
        case ChargeState::Windup:
            desiredDirection = Vector3Zero();
            targetSpeed = 0.0f;
            targetPoseDeg = -90.0f;
            usesStateTimer = true;
            timerWaitsForPose = true;
            break;
        case ChargeState::Charging:
            desiredDirection = this->chargeDirection;
            targetSpeed = this->chargeSpeed;
            targetPoseDeg = -90.0f;
            usesStateTimer = true;
            break;
        case ChargeState::Recover:
            desiredDirection = Vector3Zero();
            targetSpeed = 0.0f;
            targetPoseDeg = 0.0f;
            usesStateTimer = true;
            timerWaitsForPose = true;
            break;
        }
    }

    MovementSettings settings;
    settings.maxSpeed = targetSpeed;
    settings.facingHint = toPlayer;
    settings.lockToGround = true;
    settings.enableLean = (this->state != ChargeState::Charging);
    settings.enableBobAndSway = (this->state != ChargeState::Charging);

    if (this->isMovementDisabled() || isStunned)
    {
        desiredDirection = Vector3Zero();
        settings.maxSpeed = 0.0f;
        settings.maxAccel = 0.0f;
    }

    if (this->state == ChargeState::Charging && !isStunned)
    {
        settings.maxAccel = this->chargeSpeed * 200.0f;
        settings.decelGround = 1.0f;
        settings.decelAir = 1.0f;
        settings.zeroThreshold = 0.0f;
        settings.overrideHorizontalVelocity = true;
        settings.forcedHorizontalVelocity = Vector3Scale(this->chargeDirection, this->chargeSpeed);
    }

    this->UpdateCommonBehavior(uc, desiredDirection, delta, settings);

    if (!isStunned)
    {
        bool poseAligned = this->updatePoseTowards(targetPoseDeg, delta);

        if (usesStateTimer && this->stateTimer > 0.0f)
        {
            if (!timerWaitsForPose || poseAligned)
            {
                this->stateTimer -= delta;
                if (this->stateTimer < 0.0f)
                    this->stateTimer = 0.0f;
            }
        }

        if (this->state == ChargeState::Windup)
        {
            if (poseAligned && this->stateTimer <= 0.0f)
            {
                this->state = ChargeState::Charging;
                this->stateTimer = this->chargeDuration;
                this->poseAngularVelocityDegPerSec = 0.0f;
                if (Vector3LengthSqr(toPlayer) > 0.001f)
                {
                    this->chargeDirection = Vector3Normalize(toPlayer);
                }
                this->appliedChargeDamage = false;
            }
        }
        else if (this->state == ChargeState::Charging)
        {
            if (this->stateTimer <= 0.0f || distanceToPlayer <= 1.5f)
            {
                this->state = ChargeState::Recover;
                this->stateTimer = this->recoverDuration;
                this->poseAngularVelocityDegPerSec = 0.0f;
            }
        }
        else if (this->state == ChargeState::Recover)
        {
            if (poseAligned && this->stateTimer <= 0.0f)
            {
                this->state = ChargeState::Approaching;
            }
        }
    }

    Quaternion baseRotation = this->o.getRotation();
    Vector3 forwardDir = this->getFacingDirection();
    if (Vector3LengthSqr(forwardDir) < 0.001f)
        forwardDir = {0.0f, 0.0f, 1.0f};
    forwardDir = Vector3Normalize(forwardDir);
    Vector3 upDir = {0.0f, 1.0f, 0.0f};
    Vector3 rightDir = Vector3CrossProduct(upDir, forwardDir);
    if (Vector3LengthSqr(rightDir) < 0.001f)
        rightDir = {1.0f, 0.0f, 0.0f};
    rightDir = Vector3Normalize(rightDir);

    Quaternion tiltRotation = QuaternionIdentity();
    if (fabsf(this->chargePoseAngleDeg) > 0.01f)
    {
        tiltRotation = QuaternionFromAxisAngle(rightDir, this->chargePoseAngleDeg * DEG2RAD);
    }

    if (this->state == ChargeState::Charging && !isStunned)
    {
        Vector3 horizontalVelocity = {this->velocity.x, 0.0f, this->velocity.z};
        float currentSpeed = Vector3Length(horizontalVelocity);
        float speedFraction = (this->chargeSpeed > 0.001f) ? Clamp(currentSpeed / this->chargeSpeed, 0.0f, 1.0f) : 0.0f;
        float spinRate = Lerp(this->chargeSpinMinDegPerSec, this->chargeSpinMaxDegPerSec, speedFraction);
        this->chargeSpinAngleDeg += spinRate * delta;
        if (fabsf(this->chargeSpinAngleDeg) > 3600.0f)
        {
            this->chargeSpinAngleDeg = fmodf(this->chargeSpinAngleDeg, 360.0f);
        }
    }
    else
    {
        this->chargeSpinAngleDeg = Lerp(this->chargeSpinAngleDeg, 0.0f, Clamp(delta * 10.0f, 0.0f, 1.0f));
    }

    Quaternion spinRotation = QuaternionIdentity();
    if (fabsf(this->chargeSpinAngleDeg) > 0.01f)
    {
        Vector3 spinAxis = forwardDir;
        if (this->state == ChargeState::Charging && !isStunned)
        {
            spinAxis = {0.0f, 1.0f, 0.0f};
        }
        if (Vector3LengthSqr(spinAxis) < 0.001f)
            spinAxis = {0.0f, 0.0f, 1.0f};
        spinAxis = Vector3Normalize(spinAxis);
        spinRotation = QuaternionFromAxisAngle(spinAxis, this->chargeSpinAngleDeg * DEG2RAD);
    }

    Quaternion finalRotation = QuaternionMultiply(spinRotation, QuaternionMultiply(tiltRotation, baseRotation));
    this->o.setRotation(finalRotation);
    if (!this->isKnockbackActive())
    {
        this->snapToGroundWithRotation(finalRotation);
    }
    this->o.UpdateOBB();

    if (this->state == ChargeState::Charging && !this->appliedChargeDamage && !isStunned)
    {
        CollisionResult playerHit = Object::collided(this->o, uc.player->obj());
        if (playerHit.collided)
        {
            DamageResult damage(this->chargeDamage, playerHit);
            uc.player->damage(damage);
            Vector3 knockDir = Vector3Normalize(this->chargeDirection);
            if (Vector3LengthSqr(knockDir) < 0.0001f)
                knockDir = {0.0f, 0.0f, 1.0f};
            uc.player->applyKnockback(Vector3Scale(knockDir, this->chargeKnockbackForce), 0.35f, 3.0f);
            this->appliedChargeDamage = true;
        }
    }
    this->updateElectrocute(delta);
    // Update health dialog for charging enemy
    this->UpdateDialog(uc);
}

bool ChargingEnemy::updatePoseTowards(float targetAngleDeg, float deltaSeconds)
{
    const float angleTolerance = 0.5f;
    const float velocityTolerance = 5.0f;

    float angleDiff = targetAngleDeg - this->chargePoseAngleDeg;
    if (fabsf(angleDiff) <= angleTolerance && fabsf(this->poseAngularVelocityDegPerSec) <= velocityTolerance)
    {
        this->chargePoseAngleDeg = targetAngleDeg;
        this->poseAngularVelocityDegPerSec = 0.0f;
        return true;
    }

    float accel = (angleDiff >= 0.0f) ? this->poseRiseAccelerationDegPerSec2 : -this->poseFallAccelerationDegPerSec2;

    if ((angleDiff > 0.0f && this->poseAngularVelocityDegPerSec < 0.0f) ||
        (angleDiff < 0.0f && this->poseAngularVelocityDegPerSec > 0.0f))
    {
        float dampFactor = Clamp(deltaSeconds * 12.0f, 0.0f, 1.0f);
        this->poseAngularVelocityDegPerSec = Lerp(this->poseAngularVelocityDegPerSec, 0.0f, dampFactor);
    }

    this->poseAngularVelocityDegPerSec += accel * deltaSeconds;
    this->poseAngularVelocityDegPerSec = Clamp(this->poseAngularVelocityDegPerSec,
                                               -this->poseMaxAngularVelocityDegPerSec,
                                               this->poseMaxAngularVelocityDegPerSec);

    this->chargePoseAngleDeg += this->poseAngularVelocityDegPerSec * deltaSeconds;

    bool overshootFalling = (angleDiff < 0.0f && this->chargePoseAngleDeg <= targetAngleDeg);
    bool overshootRising = (angleDiff > 0.0f && this->chargePoseAngleDeg >= targetAngleDeg);

    if (overshootFalling || overshootRising)
    {
        this->chargePoseAngleDeg = targetAngleDeg;
        this->poseAngularVelocityDegPerSec = 0.0f;
        return true;
    }

    this->chargePoseAngleDeg = Clamp(this->chargePoseAngleDeg, -90.0f, 0.0f);
    return false;
}

bool Enemy::damage(DamageResult &dResult)
{
    int healthBefore = this->health;
    this->health -= (int)dResult.damage;
    TraceLog(LOG_ERROR, "[Enemy] damage applied: before=%d damage=%.1f after=%d obj=%p", healthBefore, dResult.damage, this->health, (void*)this);
    if (this->health <= 0)
    {
        TraceLog(LOG_ERROR, "[Enemy] DEATH: obj=%p was at %d hp, took %.1f dmg", (void*)this, healthBefore, dResult.damage);
    }
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
        this->grounded = false;
    }
    this->knockbackTimer = fmaxf(this->knockbackTimer, durationSeconds);
}

void Enemy::applyStun(float durationSeconds)
{
    this->stunTimer = fmaxf(this->stunTimer, durationSeconds);
    this->stunShakePhase = 0.0f;
    this->disableVoluntaryMovement(durationSeconds);
}

void Enemy::tickStatusTimers(float deltaSeconds)
{
    if (this->movementDisableTimer > 0.0f)
    {
        this->movementDisableTimer = fmaxf(0.0f, this->movementDisableTimer - deltaSeconds);
    }
    if (this->electrocuteTimer > 0.0f)
    {
        this->electrocuteTimer = fmaxf(0.0f, this->electrocuteTimer - deltaSeconds);
    }
    if (this->stunTimer > 0.0f)
    {
        this->stunTimer = fmaxf(0.0f, this->stunTimer - deltaSeconds);
    }
}

bool Enemy::updateStun(UpdateContext &uc)
{
    return this->stunTimer > 0.0f;
}

bool Enemy::updateElectrocute(float deltaSeconds)
{
    if (this->electrocuteTimer <= 0.0f)
        return false;

    this->electrocutePhase += deltaSeconds * 18.0f;

    // Visual electrocution: yaw-only shake (left-right). Avoid tilt so enemies don't tumble.
    float yawShakeDeg = sinf(this->electrocutePhase * 2.8f) * 8.0f;
    this->o.rotate({0, 1, 0}, yawShakeDeg);
    this->o.UpdateOBB();
    return true;
}

void Enemy::gatherObjects(std::vector<Object *> &out) const
{
    out.push_back(const_cast<Object *>(&this->o));
}

// Base draw implementation - just draws the object
void Enemy::Draw() const
{
    // Default: no custom drawing, enemy is drawn via object system
}

// Update the enemy's dialog box position/text/visibility
void Enemy::UpdateDialog(UpdateContext &uc, float verticalOffset)
{
    if (!this->healthDialog)
    {
        this->healthDialog = new DialogBox();
        this->healthDialog->setBarSize(2.5f, 0.32f);
    }

    // Position the dialog above the enemy's head
    Vector3 headPos = this->o.getPos();
    headPos.y += this->o.getSize().y * 0.5f + verticalOffset;
    this->healthDialog->setWorldPosition(headPos);
    this->healthDialog->setVisible(true);

    this->healthDialog->setFillPercent(this->getHealthPercent());
}

ShooterEnemy::ShooterEnemy() : Enemy(250)  // Sniper: 250 HP
{
    this->setMaxHealth(250);
    this->setTileType(TileType::BAMBOO_7); // Sniper uses Bamboo tiles
    TraceLog(LOG_WARNING, "[ShooterEnemy] spawned: this=%p health=%d maxHealth=%d", (void*)this, this->getHealth(), 250);
    
    // Set default bullet pattern (single bullet)
    this->bulletPattern.bulletCount = 1;
    this->bulletPattern.arcDegrees = 0.0f;
    
    // Load sun texture for bullets
    this->sunTexture = LoadTexture("sun.png");
    if (this->sunTexture.id == 0)
    {
        TraceLog(LOG_WARNING, "ShooterEnemy: Failed to load sun.png");
    }
}
// ---------------------------- SummonerEnemy ----------------------------
void SummonerEnemy::SpawnMinionGroup(UpdateContext &uc)
{
    int count = groupSize; // Fixed count of 5
    float radius = 4.0f;
    
    // Calculate minion size: summoner size / 3
    Vector3 minionSize = Vector3Scale(this->obj().size, 1.0f / 3.0f);
    // Determine room bounds for summoner so spawned minions remain inside
    Room *room = nullptr;
    BoundingBox roomBounds{};
    bool haveRoomBounds = false;
    if (uc.scene)
    {
        room = uc.scene->GetRoomContainingPosition(this->position);
        if (room)
        {
            roomBounds = room->GetBounds();
            haveRoomBounds = true;
        }
    }
    
    for (int i = 0; i < count; ++i)
    {
        float angle = (2.0f * PI) * ((float)i / (float)count);
        Vector3 offset = {cosf(angle) * radius, 0.0f, sinf(angle) * radius};
        Vector3 spawnPos = Vector3Add(this->position, offset);
        // Clamp spawn position into room bounds if available (keep a small margin)
        if (haveRoomBounds)
        {
            const float margin = 0.5f;
            spawnPos.x = fmaxf(roomBounds.min.x + margin, fminf(spawnPos.x, roomBounds.max.x - margin));
            spawnPos.z = fmaxf(roomBounds.min.z + margin, fminf(spawnPos.z, roomBounds.max.z - margin));
        }
            MinionEnemy *m = new MinionEnemy(); // Create a new MinionEnemy instance
        m->obj().size = minionSize;
        m->obj().pos = spawnPos;
        m->setPosition(spawnPos);
        
        // Copy texture from summoner
        m->obj().texture = this->obj().texture;
        m->obj().sourceRect = this->obj().sourceRect;
        m->obj().useTexture = this->obj().useTexture;
        
        // Track this minion so it can be cleaned up when summoner dies
        this->ownedMinions.push_back(m);
        uc.scene->em.addEnemy(m);
        // Small spawn particles for each minion (helps visibility)
        if (uc.scene)
        {
            uc.scene->particles.spawnExplosion(spawnPos, 8, PURPLE, 0.12f, 2.5f, 0.6f);
            uc.scene->particles.spawnRing(spawnPos, 1.0f, 8, ColorAlpha(PURPLE, 200), 2.0f, true);
        }
    }
}

void SummonerEnemy::CleanupMinions(UpdateContext &uc)
{
    // Remove all minions from the enemy manager
    for (MinionEnemy* minion : this->ownedMinions)
    {
        if (minion)
        {
            uc.scene->em.RemoveEnemy(minion);
        }
    }
    this->ownedMinions.clear();
}

SummonerEnemy::~SummonerEnemy()
{
    // Texture will be cleaned up by UIManager, don't unload it here
    if (this->spiralParticleTexture.id != 0 && IsWindowReady())
    {
        UnloadTexture(this->spiralParticleTexture);
        this->spiralParticleTexture.id = 0;
    }
}

void SummonerEnemy::OnDeath(UpdateContext &uc)
{
    // Cleanup owned minions when summoner dies
    this->CleanupMinions(uc);
}

void SummonerEnemy::UpdateSummonAnimation(UpdateContext &uc, float delta)
{
    // Emit particles during animation
    EmitSummonParticles(this->position, (summonState != SummonState::Idle) ? 1.0f : 0.0f);
    
    switch (summonState)
    {
    case SummonState::Idle:
        // Normal countdown to next summon
        spawnTimer += delta;
        if (spawnTimer >= spawnInterval)
        {
            spawnTimer = 0.0f;
            summonState = SummonState::Ascending;
            animationTimer = 0.0f;
            // Store starting position for spiral animation
            startHeight = this->obj().pos.y;
            startAnimX = this->obj().pos.x;
            startAnimZ = this->obj().pos.z;
        }
        break;

    case SummonState::Ascending:
    {
        animationTimer += delta;
        float progress = animationTimer / ascendDuration;
        
        if (progress >= 1.0f)
        {
            // Transition to descending at peak
            summonState = SummonState::Descending;
            animationTimer = 0.0f;
            progress = 1.0f;
        }
        
        // Spiral upward: height follows arc, position spirals around center
        float heightFactor = sinf(progress * PI * 0.5f); // Smooth easing from 0 to 1
        float newHeight = startHeight + (jumpHeight * heightFactor);
        float spiralAngle = progress * twirls * PI * 2.0f;
        
        // Calculate spiral offset from base position
        float spiralX = cosf(spiralAngle) * spiralRadius * progress;
        float spiralZ = sinf(spiralAngle) * spiralRadius * progress;
        
        // Apply animation position relative to starting position
        Vector3 newPos = this->obj().pos;
        newPos.x = startAnimX + spiralX;
        newPos.y = startHeight + (jumpHeight * heightFactor);
        newPos.z = startAnimZ + spiralZ;
        this->obj().pos = newPos;
        this->position = newPos;
        
        // Rotate during ascent (spin around Y axis)
        Vector3 upAxis = {0.0f, 1.0f, 0.0f};
        Quaternion rotQuat = QuaternionFromAxisAngle(upAxis, spiralAngle);
        this->obj().rotation = rotQuat;
        // Emit spiral particles around the summoner during ascent (denser, smaller)
        if (uc.scene)
        {
            // spawnSpiral(center, radius, count, color, height, speed)
            uc.scene->particles.spawnSpiral(this->position, spiralRadius * 0.5f, 18, PURPLE, jumpHeight * progress, 1.2f);
        }
        break;
    }

    case SummonState::Descending:
    {
        animationTimer += delta;
        float progress = animationTimer / descendDuration;
        
        if (progress >= 1.0f)
        {
            // Transition to summoning peak
            summonState = SummonState::Summoning;
            animationTimer = 0.0f;
            this->obj().pos.y = startHeight;
            this->position.y = startHeight;
            this->obj().rotation = QuaternionIdentity();
            progress = 1.0f;
        }
        
        // Spiral downward: height falls from peak, continues spiral
        float heightFactor = cosf(progress * PI * 0.5f); // Smooth easing from 1 to 0
        float newHeight = startHeight + (jumpHeight * heightFactor);
        float spiralAngle = (1.0f - progress) * twirls * PI * 2.0f;
        
        // Spiral continues during descent
        float spiralX = cosf(spiralAngle) * spiralRadius * (1.0f - progress);
        float spiralZ = sinf(spiralAngle) * spiralRadius * (1.0f - progress);
        
        // Apply animation position
        Vector3 newPos = this->obj().pos;
        newPos.x = startAnimX + spiralX;
        newPos.y = startHeight + (jumpHeight * heightFactor);
        newPos.z = startAnimZ + spiralZ;
        this->obj().pos = newPos;
        this->position = newPos;
        
        // Continue rotation
        Vector3 upAxis = {0.0f, 1.0f, 0.0f};
        Quaternion rotQuat = QuaternionFromAxisAngle(upAxis, spiralAngle);
        this->obj().rotation = rotQuat;
        // Emit spiral particles during descent as well
        if (uc.scene)
        {
            uc.scene->particles.spawnSpiral(this->position, spiralRadius * 0.5f, 14, PURPLE, jumpHeight * (1.0f - progress), 1.0f);
        }
        break;
    }

    case SummonState::Summoning:
    {
        animationTimer += delta;
        // Emit spiral particles while holding at peak
        if (uc.scene)
        {
            uc.scene->particles.spawnSpiral(this->position, spiralRadius * 0.5f, 20, PURPLE, 0.6f, 0.6f);
        }
        if (animationTimer >= summonPeakDuration)
        {
            // Spawn the minions
            this->SpawnMinionGroup(uc);
            
            // Spawn explosion of purple particles when minions appear
            if (uc.scene)
            {
                uc.scene->particles.spawnExplosion(this->position, 30, PURPLE, 0.3f, 5.0f, 1.0f);
                uc.scene->particles.spawnRing(this->position, 3.0f, 20, ColorAlpha(PURPLE, 200), 4.0f, true);
            }
            
            // Return to idle
            summonState = SummonState::Idle;
            animationTimer = 0.0f;
            this->obj().rotation = QuaternionIdentity();
        }
        else
        {
            // Brief pause at peak with rotation
            float peakRotation = (animationTimer / summonPeakDuration) * 15.0f * DEG2RAD;
            Vector3 upAxis = {0.0f, 1.0f, 0.0f};
            Quaternion rotQuat = QuaternionFromAxisAngle(upAxis, peakRotation);
            this->obj().rotation = rotQuat;
        }
        break;
    }
    }
}

void SummonerEnemy::EmitSummonParticles(const Vector3 &summonPos, float intensity)
{
    // Load purple particle texture on first use
    if (this->spiralParticleTexture.id == 0)
    {
        this->spiralParticleTexture = LoadTexture("kenney_particle-pack/PNG (Transparent)/magic_02.png");
        if (this->spiralParticleTexture.id == 0)
        {
            TraceLog(LOG_WARNING, "Failed to load spiral particle texture");
            return;
        }
    }
}

void SummonerEnemy::Draw() const
{
    // Draw base enemy
    Enemy::Draw();
    // Particles are now handled by the particle system in UpdateBody
}

void SummonerEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->tickStatusTimers(delta);
    bool isStunned = this->updateStun(uc);

    // If stunned, interrupt any ongoing summon animation so physics can take over.
    if (isStunned && summonState != SummonState::Idle)
    {
        summonState = SummonState::Idle;
    }
    
    // Animation logic runs only if not stunned.
    if (!isStunned && !this->isMovementDisabled())
    {
        UpdateSummonAnimation(uc, delta);
    }
    
    // If the enemy is animating, its position is controlled by the animation, not physics.
    // So we can return early.
    if (summonState != SummonState::Idle)
    {
        this->UpdateDialog(uc);
        // We don't call UpdateCommonBehavior because the animation is kinematic.
        return;
    }
    
    // --- From here on, we are NOT animating, so normal physics/AI applies ---

    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    float dist = Vector3Length(toPlayer);
    Vector3 desiredDir = Vector3Zero();

    Enemy::MovementSettings settings;
    settings.lockToGround = true;
    settings.maxSpeed = 8.0f;
    settings.maxAccel = MAX_ACCEL;
    settings.decelGround = FRICTION;
    settings.decelAir = AIR_DRAG;

    // AI behavior only if not stunned/disabled
    if (!isStunned && !this->isMovementDisabled())
    {
        if (dist < retreatDistance)
        {
            desiredDir = Vector3Normalize(Vector3Negate(toPlayer));
            settings.facingHint = Vector3Negate(desiredDir);
        }
        else
        {
            settings.facingHint = toPlayer;
        }
    }
    
    this->UpdateCommonBehavior(uc, desiredDir, delta, settings);
    this->updateElectrocute(delta);
    this->UpdateDialog(uc);
}

void ShooterEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->tickStatusTimers(delta);
    bool isStunned = this->updateStun(uc);

    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    float distance = Vector3Length(toPlayer);

    Vector3 aimDir = {0.0f, 0.0f, 0.0f};
    bool hasLineOfSight = false;
    MovementCommand command{};

    if (!isStunned && !this->isMovementDisabled())
    {
        hasLineOfSight = this->findShotDirection(uc, aimDir);
        bool withinRange = this->isWithinPreferredRange(distance);

        if (this->phase == Phase::FindPosition)
        {
            command = this->FindMovement(uc, toPlayer, distance, hasLineOfSight, delta);
            if (withinRange && hasLineOfSight)
            {
                this->phase = Phase::Shooting;
            }
        }
        else // Shooting phase
        {
            if (!withinRange || !hasLineOfSight)
            {
                this->phase = Phase::FindPosition;
                command = this->FindMovement(uc, toPlayer, distance, hasLineOfSight, delta);
            }
        }
    }

    MovementSettings settings;
    settings.maxSpeed = command.speed;
    settings.facingHint = toPlayer;
    settings.lockToGround = true;
    settings.enableLean = command.speed > 0.1f;
    settings.enableBobAndSway = command.speed > 0.1f;

    if (this->isMovementDisabled() || isStunned)
    {
        command.direction = Vector3Zero();
        settings.maxSpeed = 0.0f;
        settings.maxAccel = 0.0f;
    }

    this->UpdateCommonBehavior(uc, command.direction, delta, settings);

    if (!isStunned)
    {
        Vector3 muzzle = this->position;
        muzzle.y += this->muzzleHeight;

        if (this->phase == Phase::Shooting)
        {
            this->HandleShooting(delta, muzzle, aimDir, hasLineOfSight);
        }
        else
        {
            this->fireCooldown = fmaxf(this->fireCooldown - delta, 0.0f);
        }
    }

    this->updateBullets(uc, delta);
    this->updateElectrocute(delta);
    // Update health dialog
    this->UpdateDialog(uc);
}

ShooterEnemy::MovementCommand ShooterEnemy::FindMovement(UpdateContext &uc, const Vector3 &toPlayer, float distance, bool hasLineOfSight, float deltaSeconds)
{
    MovementCommand command{};

    Vector3 planar = toPlayer;
    float planarLenSq = Vector3LengthSqr(planar);
    if (planarLenSq > 0.0001f)
    {
        planar = Vector3Normalize(planar);
    }
    else
    {
        planar = Vector3Zero();
    }

    if (distance > this->maxFiringDistance)
    {
        command.direction = planar;
        command.speed = this->approachSpeed;
        this->losRepositionTimer = 0.0f;
        return command;
    }

    if (distance < this->retreatDistance)
    {
        command.direction = Vector3Scale(planar, -1.0f);
        command.speed = this->retreatSpeed;
        this->losRepositionTimer = 0.0f;
        return command;
    }

    if (!hasLineOfSight)
    {
        this->losRepositionTimer += deltaSeconds;
        if (!this->hasRepositionGoal || this->repositionCooldown <= 0.0f || this->losRepositionTimer >= this->strafeSwitchInterval)
        {
            if (this->SelectRepositionGoal(uc, planar, distance))
            {
                this->repositionCooldown = this->repositionCooldownDuration;
                this->losRepositionTimer = 0.0f;
            }
        }
        else
        {
            this->repositionCooldown -= deltaSeconds;
        }

        if (this->hasRepositionGoal)
        {
            Vector3 toGoal = Vector3Subtract(this->losRepositionGoal, this->position);
            toGoal.y = 0.0f;
            if (Vector3LengthSqr(toGoal) > 0.25f)
            {
                command.direction = Vector3Normalize(toGoal);
                command.speed = this->approachSpeed;
            }
            else
            {
                this->hasRepositionGoal = false;
            }
        }

        return command;
    }

    this->losRepositionTimer = 0.0f;
    this->hasRepositionGoal = false;
    this->repositionCooldown = 0.0f;
    return command;
}

bool ShooterEnemy::isWithinPreferredRange(float distance) const
{
    return distance <= this->maxFiringDistance && distance >= this->retreatDistance;
}

void ShooterEnemy::HandleShooting(float deltaSeconds, const Vector3 &muzzlePosition, const Vector3 &aimDirection, bool hasLineOfSight)
{
    this->fireCooldown = fmaxf(this->fireCooldown - deltaSeconds, 0.0f);

    if (!hasLineOfSight)
    {
        return;
    }

    if (this->fireCooldown > 0.0f)
    {
        return;
    }

    if ((int)this->bullets.size() >= this->maxActiveBullets)
    {
        return;
    }

    // Spawn bullets according to pattern
    if (this->bulletPattern.bulletCount <= 1 || this->bulletPattern.arcDegrees <= 0.0f)
    {
        // Single bullet - shoot straight
        this->spawnBullet(muzzlePosition, aimDirection);
    }
    else
    {
        // Multiple bullets in a fan pattern
        Vector3 aimNormalized = Vector3Normalize(aimDirection);
        float halfArc = this->bulletPattern.arcDegrees * 0.5f * DEG2RAD;
        
        // Calculate right vector perpendicular to aim direction (in horizontal plane)
        Vector3 up = {0.0f, 1.0f, 0.0f};
        Vector3 right = Vector3Normalize(Vector3CrossProduct(aimNormalized, up));
        if (Vector3LengthSqr(right) < 0.0001f)
        {
            // If aiming straight up/down, use a default right vector
            right = {1.0f, 0.0f, 0.0f};
        }
        
        // Spawn bullets spread across the arc
        for (int i = 0; i < this->bulletPattern.bulletCount; ++i)
        {
            float angle;
            if (this->bulletPattern.bulletCount == 1)
            {
                angle = 0.0f;
            }
            else
            {
                // Distribute bullets evenly across the arc
                float t = (float)i / (float)(this->bulletPattern.bulletCount - 1);
                angle = Lerp(-halfArc, halfArc, t);
            }
            
            // Rotate aim direction around the up axis by the angle
            Vector3 bulletDir = Vector3RotateByAxisAngle(aimNormalized, up, angle);
            this->spawnBullet(muzzlePosition, bulletDir);
        }
    }
    
    this->fireCooldown = this->fireInterval;
}

bool ShooterEnemy::HasLineOfSightFromPosition(const Vector3 &origin, UpdateContext &uc) const
{
    Vector3 muzzle = origin;
    muzzle.y = origin.y + this->muzzleHeight;

    Vector3 targetPoint = uc.player->getCamera().position;
    Vector3 toTarget = Vector3Subtract(targetPoint, muzzle);
    float distance = Vector3Length(toTarget);
    if (distance < 0.5f)
    {
        return false;
    }

    Vector3 dir = Vector3Scale(toTarget, 1.0f / distance);
    float probeRadius = fmaxf(this->bulletRadius * 0.4f, 0.08f);
    Vector3 losStart = Vector3Add(muzzle, Vector3Scale(dir, probeRadius * 1.5f));
    return this->hasLineOfFire(losStart, targetPoint, uc, probeRadius);
}

bool ShooterEnemy::SelectRepositionGoal(UpdateContext &uc, const Vector3 &planarToPlayer, float distanceToPlayer)
{
    Vector3 dir = planarToPlayer;
    dir.y = 0.0f;
    if (Vector3LengthSqr(dir) < 0.0001f)
    {
        dir = {0.0f, 0.0f, 1.0f};
    }
    else
    {
        dir = Vector3Normalize(dir);
    }

    static const float offsetAnglesDeg[] = {90.0f, -90.0f, 60.0f, -60.0f, 120.0f, -120.0f};
    float desiredDistance = Clamp(distanceToPlayer, this->retreatDistance + 2.0f, this->maxFiringDistance - 4.0f);
    Vector3 playerPos = uc.player->pos();
    float baseY = this->position.y;

    auto rotateY = [](Vector3 v, float degrees) {
        float radians = degrees * DEG2RAD;
        float cs = cosf(radians);
        float sn = sinf(radians);
        return Vector3{
            v.x * cs - v.z * sn,
            0.0f,
            v.x * sn + v.z * cs};
    };

    for (float angle : offsetAnglesDeg)
    {
        Vector3 candidateDir = rotateY(dir, angle);
        if (Vector3LengthSqr(candidateDir) < 0.0001f)
        {
            continue;
        }
        candidateDir = Vector3Normalize(candidateDir);
        Vector3 desiredPos = Vector3Subtract(playerPos, Vector3Scale(candidateDir, desiredDistance));
        desiredPos.y = baseY;

        if (this->HasLineOfSightFromPosition(desiredPos, uc))
        {
            this->losRepositionGoal = desiredPos;
            this->hasRepositionGoal = true;
            return true;
        }
    }

    this->hasRepositionGoal = false;
    return false;
}

bool ShooterEnemy::findShotDirection(UpdateContext &uc, Vector3 &outDir) const
{
    Vector3 muzzle = this->position;
    muzzle.y += this->muzzleHeight;
    const Camera &playerCamera = uc.player->getCamera();
    Vector3 targetPoint = playerCamera.position;
    Vector3 toPlayer = Vector3Subtract(targetPoint, muzzle);
    float distance = Vector3Length(toPlayer);
    if (distance < 0.001f)
        return false;

    float losProbeRadius = fmaxf(this->bulletRadius * 0.4f, 0.08f);
    Vector3 dir = Vector3Scale(toPlayer, 1.0f / distance);
    Vector3 losStart = Vector3Add(muzzle, Vector3Scale(dir, losProbeRadius * 1.5f));
    Vector3 endPoint = targetPoint;

    if (!this->hasLineOfFire(losStart, endPoint, uc, losProbeRadius))
        return false;

    outDir = dir;
    return true;
}

bool ShooterEnemy::hasLineOfFire(const Vector3 &start, const Vector3 &end, UpdateContext &uc, float probeRadius) const
{
    float losRadius = fmaxf(probeRadius, 0.05f);
    float ignoreDistance = fmaxf(losRadius * 1.5f, 0.2f);
    auto staticObjects = uc.scene->getStaticObjects();
    for (auto *obj : staticObjects)
    {
        if (!obj)
            continue;
        float hitDistance = 0.0f;
        if (CheckLineSegmentVsOBB(start, end, losRadius, &obj->obb, &hitDistance))
        {
            if (hitDistance <= ignoreDistance)
            {
                continue;
            }
            return false;
        }
    }

    if (uc.scene->CheckDecorationSweep(start, end, losRadius))
    {
        return false;
    }

    return true;
}

void ShooterEnemy::spawnBullet(const Vector3 &origin, const Vector3 &dir)
{
    Bullet bullet;
    bullet.position = origin;
    bullet.velocity = Vector3Scale(Vector3Normalize(dir), this->bulletSpeed);
    bullet.radius = this->bulletRadius;
    bullet.remainingLife = this->bulletLifetime;
    bullet.visual = Object();
    bullet.visual.setAsSphere(bullet.radius);
    bullet.visual.pos = origin;
    bullet.visual.tint = {255, 255, 255, 255};
    bullet.visual.visible = true;
    
    // Apply sun texture if available
    if (this->sunTexture.id != 0)
    {
        bullet.visual.useTexture = true;
        bullet.visual.texture = &this->sunTexture;
        bullet.visual.sourceRect = {0.0f, 0.0f, (float)this->sunTexture.width, (float)this->sunTexture.height};
    }
    
    bullet.visual.UpdateOBB();

    this->bullets.push_back(bullet);
}

void ShooterEnemy::updateBullets(UpdateContext &uc, float deltaSeconds)
{
    for (auto &bullet : this->bullets)
    {
        bullet.remainingLife -= deltaSeconds;
        bullet.position = Vector3Add(bullet.position, Vector3Scale(bullet.velocity, deltaSeconds));
        bullet.visual.pos = bullet.position;
        bullet.visual.UpdateOBB();
        
        // Spawn trailing particles for Minecraft look
        if (uc.scene)
        {
            uc.scene->particles.spawnExplosion(bullet.position, 1, ORANGE, 0.15f, 0.5f, 0.1f);
        }
    }

    auto removeIt = std::remove_if(this->bullets.begin(), this->bullets.end(), [&](Bullet &bullet) {
        if (bullet.remainingLife <= 0.0f)
            return true;

        if (CheckCollisionSphereVsOBB(bullet.position, bullet.radius, &uc.player->obj().obb))
        {
            CollisionResult directHit = Object::collided(bullet.visual, uc.player->obj());
            DamageResult damage(this->bulletDamage, directHit);
            uc.player->damage(damage);
            Vector3 knockDir = Vector3Normalize(bullet.velocity);
            if (Vector3LengthSqr(knockDir) < 0.0001f)
                knockDir = {0.0f, 0.0f, 1.0f};
            uc.player->applyKnockback(Vector3Scale(knockDir, 5.0f), 0.2f, 0.0f);
            
            // Spawn impact particles
            if (uc.scene)
            {
                uc.scene->particles.spawnExplosion(bullet.position, 15, ORANGE, 0.2f, 3.0f, 0.8f);
            }
            return true;
        }

        auto collisions = Object::collided(bullet.visual, uc.scene);
        for (auto &hit : collisions)
        {
            if (hit.with == this)
                continue;

            if (hit.with && hit.with->category() == ENTITY_PLAYER)
            {
                DamageResult damage(this->bulletDamage, hit);
                uc.player->damage(damage);
                Vector3 knockDir = Vector3Normalize(bullet.velocity);
                if (Vector3LengthSqr(knockDir) < 0.0001f)
                    knockDir = {0.0f, 0.0f, 1.0f};
                uc.player->applyKnockback(Vector3Scale(knockDir, 5.0f), 0.2f, 0.0f);
                
                // Spawn impact particles
                if (uc.scene)
                {
                    uc.scene->particles.spawnExplosion(bullet.position, 15, ORANGE, 0.2f, 3.0f, 0.8f);
                }
                return true;
            }

            // Ignore collisions with player projectiles
            if (hit.with && hit.with->category() == ENTITY_PROJECTILE)
            {
                continue;
            }

            // Spawn impact particles on wall/environment hit
            if (uc.scene)
            {
                uc.scene->particles.spawnExplosion(bullet.position, 10, ORANGE, 0.2f, 2.0f, 0.6f);
            }
            return true; // hit environment or other entity
        }

        return false;
    });

    this->bullets.erase(removeIt, this->bullets.end());
}

ShooterEnemy::~ShooterEnemy()
{
    if (this->sunTexture.id != 0)
    {
        if (IsWindowReady())
        {
            UnloadTexture(this->sunTexture);
        }
        this->sunTexture.id = 0;
    }
}

void ShooterEnemy::gatherObjects(std::vector<Object *> &out) const
{
    Enemy::gatherObjects(out);
    for (const auto &bullet : this->bullets)
    {
        out.push_back(const_cast<Object *>(&bullet.visual));
    }
}

// ---------------------------- SupportEnemy ----------------------------

// ======================== SupportEnemy ========================

Enemy* SupportEnemy::FindAllyToHideBehind(UpdateContext &uc)
{
    // Find allies within normal search radius
    std::vector<Entity *> enemies = uc.scene->em.getEntities(ENTITY_ENEMY);
    std::vector<Enemy*> candidateAllies;
    
    for (Entity *entity : enemies)
    {
        if (!entity || entity == this) continue;
        
        Enemy *ally = dynamic_cast<Enemy*>(entity);
        if (!ally) continue;
        
        // Skip minions
        if (dynamic_cast<MinionEnemy*>(ally)) continue;
        
        float dist = Vector3Distance(this->position, ally->pos());
        if (dist <= normalSearchRadius)
        {
            candidateAllies.push_back(ally);
        }
    }
    
    // Priority 1: Find a tank (ChargingEnemy)
    for (Enemy* ally : candidateAllies)
    {
        if (dynamic_cast<ChargingEnemy*>(ally))
        {
            return ally;
        }
    }
    
    // Priority 2: Pick a random ally from candidates
    if (!candidateAllies.empty())
    {
        int randomIdx = GetRandomValue(0, (int)candidateAllies.size() - 1);
        return candidateAllies[randomIdx];
    }
    
    // Priority 3: Find closest enemy (even if far)
    Enemy* closestAlly = nullptr;
    float closestDist = 999999.0f;
    
    for (Entity *entity : enemies)
    {
        if (!entity || entity == this) continue;
        
        Enemy *ally = dynamic_cast<Enemy*>(entity);
        if (!ally) continue;
        
        // Skip minions
        if (dynamic_cast<MinionEnemy*>(ally)) continue;
        
        float dist = Vector3Distance(this->position, ally->pos());
        if (dist < closestDist)
        {
            closestDist = dist;
            closestAlly = ally;
        }
    }
    
    return closestAlly;
}

Enemy* SupportEnemy::FindBestTarget(UpdateContext &uc, bool forHealing)
{
    std::vector<Entity *> enemies = uc.scene->em.getEntities(ENTITY_ENEMY);
    Enemy* bestTarget = nullptr;
    
    if (forHealing)
    {
        // Find lowest HP ally below healing threshold within search radius
        float lowestHealthPercent = 1.0f;
        
        for (Entity *entity : enemies)
        {
            if (!entity || entity == this) continue;
            
            Enemy *ally = dynamic_cast<Enemy*>(entity);
            if (!ally) continue;
            
            // Skip minions
            if (dynamic_cast<MinionEnemy*>(ally)) continue;
            
            float dist = Vector3Distance(this->position, ally->pos());
            if (dist > actionSearchRadius) continue;
            
            float healthPercent = (float)ally->getHealth() / (float)ally->getMaxHealth();
            if (healthPercent >= healingThreshold) continue;
            
            if (healthPercent < lowestHealthPercent)
            {
                lowestHealthPercent = healthPercent;
                bestTarget = ally;
            }
        }
    }
    else
    {
        // Find any ally to buff (prioritize lowest health)
        float lowestHealthPercent = 1.0f;
        
        for (Entity *entity : enemies)
        {
            if (!entity || entity == this) continue;
            
            Enemy *ally = dynamic_cast<Enemy*>(entity);
            if (!ally) continue;
            
            // Skip minions
            float dist = Vector3Distance(this->position, ally->pos());
            if (dist > actionSearchRadius) continue;
            
            float healthPercent = (float)ally->getHealth() / (float)ally->getMaxHealth();
            if (healthPercent < lowestHealthPercent)
            {
                lowestHealthPercent = healthPercent;
                bestTarget = ally;
            }
        }
    }
    
    return bestTarget;
}

Vector3 SupportEnemy::CalculateHidePosition(UpdateContext &uc, Enemy* allyToHideBehind)
{
    if (!allyToHideBehind)
        return this->position;
    
    // Calculate line from player to ally
    Vector3 playerPos = uc.player->pos();
    Vector3 allyPos = allyToHideBehind->pos();
    Vector3 playerToAlly = Vector3Subtract(allyPos, playerPos);
    playerToAlly.y = 0.0f;
    
    float distToAlly = Vector3Length(playerToAlly);
    if (distToAlly < 0.1f)
    {
        // If player and ally are at same position, just retreat
        return Vector3Add(allyPos, Vector3Scale({0.0f, 0.0f, 1.0f}, normalHideDistance));
    }
    
    // Normalize and extend beyond ally
    Vector3 hideDir = Vector3Normalize(playerToAlly);
    Vector3 hidePos = Vector3Add(allyPos, Vector3Scale(hideDir, normalHideDistance));
    
    return hidePos;
}

void SupportEnemy::UpdateNormalMode(UpdateContext &uc, const Vector3 &toPlayer)
{
    float delta = GetFrameTime();
    float playerDist = Vector3Length(toPlayer);
    
    Vector3 desiredDir = Vector3Zero();
    Enemy::MovementSettings settings;
    settings.lockToGround = true;
    settings.maxSpeed = 3.0f;
    settings.maxAccel = MAX_ACCEL;
    settings.decelGround = FRICTION;
    settings.decelAir = AIR_DRAG;
    
    // Check for targets to heal or buff
    Enemy* healTarget = FindBestTarget(uc, true);
    Enemy* buffTarget = FindBestTarget(uc, false);
    
    // Decide: heal takes priority over buff
    if (healTarget)
    {
        this->targetAlly = healTarget;
        this->mode = SupportMode::Heal;
        this->actionTimer = 0.0f;
        return; // Will be handled by UpdateHealMode next frame
    }
    else if (buffTarget)
    {
        this->targetAlly = buffTarget;
        this->mode = SupportMode::Buff;
        this->actionTimer = 0.0f;
        return; // Will be handled by UpdateBuffMode next frame
    }
    
    // Normal mode: hide behind allies or retreat
    this->targetAlly = FindAllyToHideBehind(uc);
    
    if (this->targetAlly)
    {
        // Move toward hide position
        Vector3 hidePos = CalculateHidePosition(uc, this->targetAlly);
        Vector3 toHidePos = Vector3Subtract(hidePos, this->position);
        toHidePos.y = 0.0f;
        float hideDist = Vector3Length(toHidePos);
        
        if (hideDist > 0.5f)
        {
            desiredDir = Vector3Normalize(toHidePos);
            settings.facingHint = toPlayer;
        }
    }
    else if (playerDist < retreatDistance)
    {
        // No allies nearby, retreat from player
        desiredDir = Vector3Normalize(Vector3Negate(toPlayer));
        settings.facingHint = Vector3Negate(desiredDir);
    }
    else
    {
        // Idle
        desiredDir = Vector3Zero();
        settings.facingHint = toPlayer;
    }

    if (this->isMovementDisabled())
    {
        desiredDir = Vector3Zero();
        settings.maxSpeed = 0.0f;
        settings.maxAccel = 0.0f;
    }
    
    this->UpdateCommonBehavior(uc, desiredDir, delta, settings);
    this->updateElectrocute(delta);
    this->UpdateDialog(uc);
}

void SupportEnemy::UpdateHealMode(UpdateContext &uc)
{
    float delta = GetFrameTime();
    
    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    
    Vector3 desiredDir = Vector3Zero();
    Enemy::MovementSettings settings;
    settings.lockToGround = true;
    settings.maxSpeed = 3.0f;
    settings.maxAccel = MAX_ACCEL;
    settings.decelGround = FRICTION;
    settings.decelAir = AIR_DRAG;
    
    if (!this->targetAlly)
    {
        // Target died, go back to normal mode
        this->mode = SupportMode::Normal;
        this->actionTimer = 0.0f;
        this->actionCooldownTimer = 0.0f;
        UpdateNormalMode(uc, toPlayer);
        return;
    }
    
    // Move toward target until within stand distance
    Vector3 toTarget = Vector3Subtract(this->targetAlly->pos(), this->position);
    toTarget.y = 0.0f;
    float targetDist = Vector3Length(toTarget);
    
    if (targetDist > actionStandDistance)
    {
        desiredDir = Vector3Normalize(toTarget);
        settings.facingHint = desiredDir;
        this->actionTimer = 0.0f; // Reset charge timer while moving
        this->chargeParticleTimer = 0.0f;
    }
    else
    {
        // In range, charge up and heal
        desiredDir = Vector3Zero();
        this->actionTimer += delta;
        // Throttle charging particles while standing still
        if (uc.scene)
        {
            const float emitInterval = 0.08f; // seconds between particle emissions
            this->chargeParticleTimer += delta;
            if (this->chargeParticleTimer >= emitInterval)
            {
                Vector3 healDir = Vector3Subtract(this->targetAlly->pos(), this->position);
                // Stronger burst toward target
                uc.scene->particles.spawnDirectional(this->position, healDir, 2, GOLD, 1.6f, 0.18f);
                uc.scene->particles.spawnExplosion(this->targetAlly->pos(), 2, YELLOW, 0.16f * uc.scene->particles.globalSizeMultiplier, 0.8f, 0.14f);
                // Multiple concentric rings around the support to be clearly visible
                uc.scene->particles.spawnRing(this->position, 8.0f, 10, SKYBLUE, 0.9f, true);
                uc.scene->particles.spawnRing(this->position, 12.0f, 14, SKYBLUE, 0.7f, true);
                // Accent the target with rings
                uc.scene->particles.spawnRing(this->targetAlly->pos(), 6.0f, 10, YELLOW, 0.8f, true);
                this->chargeParticleTimer -= emitInterval;
            }
        }
        
        // After charging, apply heal
        if (this->actionTimer >= actionChargeTime)
        {
            // Apply fixed heal amount at the instant the heal is fully charged
            int healAmount = 200; // fixed heal per request
            this->targetAlly->Heal(healAmount);
            
            // Emit heal impact particles
            if (uc.scene)
            {
                uc.scene->particles.spawnExplosion(this->targetAlly->pos(), 20, YELLOW, 0.25f, 3.0f, 0.8f);
                uc.scene->particles.spawnRing(this->targetAlly->pos(), 3.0f, 16, ColorAlpha(GOLD, 200), 2.5f, true);
            }
            
            // Go into cooldown
            this->mode = SupportMode::Normal;
            this->actionCooldownTimer = actionCooldown;
            this->actionTimer = 0.0f;
            this->targetAlly = nullptr;
            this->chargeParticleTimer = 0.0f;
        }
    }

    if (this->isMovementDisabled())
    {
        desiredDir = Vector3Zero();
        settings.maxSpeed = 0.0f;
        settings.maxAccel = 0.0f;
    }

    this->UpdateCommonBehavior(uc, desiredDir, delta, settings);
    this->updateElectrocute(delta);
    this->UpdateDialog(uc);
}

void SupportEnemy::UpdateBuffMode(UpdateContext &uc)
{
    float delta = GetFrameTime();
    
    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    
    Vector3 desiredDir = Vector3Zero();
    Enemy::MovementSettings settings;
    settings.lockToGround = true;
    settings.maxSpeed = 3.0f;
    settings.maxAccel = MAX_ACCEL;
    settings.decelGround = FRICTION;
    settings.decelAir = AIR_DRAG;
    
    if (!this->targetAlly)
    {
        // Target died, go back to normal mode
        this->mode = SupportMode::Normal;
        this->actionTimer = 0.0f;
        this->actionCooldownTimer = 0.0f;
        UpdateNormalMode(uc, toPlayer);
        return;
    }
    
    // Move toward target until within stand distance
    Vector3 toTarget = Vector3Subtract(this->targetAlly->pos(), this->position);
    toTarget.y = 0.0f;
    float targetDist = Vector3Length(toTarget);
    
    if (targetDist > actionStandDistance)
    {
        desiredDir = Vector3Normalize(toTarget);
        settings.facingHint = desiredDir;
        this->actionTimer = 0.0f; // Reset charge timer while moving
        this->chargeParticleTimer = 0.0f;
    }
    else
    {
        // In range, charge up and buff
        desiredDir = Vector3Zero();
        this->actionTimer += delta;
        // Throttle charging particles while standing still
        if (uc.scene)
        {
            const float emitInterval = 0.08f;
            this->chargeParticleTimer += delta;
            if (this->chargeParticleTimer >= emitInterval)
            {
                // Add visible rings around support
                uc.scene->particles.spawnRing(this->position, 10.0f, 12, SKYBLUE, 1.0f, true);
                uc.scene->particles.spawnRing(this->position, 14.0f, 16, SKYBLUE, 0.8f, true);
                // Accent target with rings and small explosion dots
                uc.scene->particles.spawnRing(this->targetAlly->pos(), 6.5f, 10, SKYBLUE, 0.9f, true);
                uc.scene->particles.spawnExplosion(this->targetAlly->pos(), 2, SKYBLUE, 0.14f * uc.scene->particles.globalSizeMultiplier, 0.7f, 0.12f);
                uc.scene->particles.spawnExplosion(this->targetAlly->pos(), 2, WHITE, 0.08f * uc.scene->particles.globalSizeMultiplier, 0.5f, 0.08f);
                this->chargeParticleTimer -= emitInterval;
            }
        }
        
        // After charging, apply buff
        if (this->actionTimer >= actionChargeTime)
        {
            // Apply buff burst
            if (uc.scene)
            {
                uc.scene->particles.spawnExplosion(this->targetAlly->pos(), 25, SKYBLUE, 0.25f, 4.0f, 0.9f);
                uc.scene->particles.spawnRing(this->targetAlly->pos(), 3.5f, 20, ColorAlpha(WHITE, 200), 3.0f, true);
            }
            
            // Go into cooldown
            this->mode = SupportMode::Normal;
            this->actionCooldownTimer = actionCooldown;
            this->actionTimer = 0.0f;
            this->targetAlly = nullptr;
            this->chargeParticleTimer = 0.0f;
        }
    }

    if (this->isMovementDisabled())
    {
        desiredDir = Vector3Zero();
        settings.maxSpeed = 0.0f;
        settings.maxAccel = 0.0f;
    }

    this->UpdateCommonBehavior(uc, desiredDir, delta, settings);
    this->updateElectrocute(delta);
    this->UpdateDialog(uc);
}

void SupportEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->tickStatusTimers(delta);
    bool isStunned = this->updateStun(uc);

    // If stunned, the enemy should do nothing but be subject to physics.
    if (isStunned || this->isMovementDisabled())
    {
        // If stunned during an action, switch back to normal mode.
        if (this->mode != SupportMode::Normal)
        {
            this->mode = SupportMode::Normal;
            this->actionTimer = 0.0f;
            this->targetAlly = nullptr;
        }

        Enemy::MovementSettings settings;
        settings.maxSpeed = 0.0f;
        settings.maxAccel = 0.0f;
        this->UpdateCommonBehavior(uc, Vector3Zero(), delta, settings);
        this->updateElectrocute(delta);
        this->UpdateDialog(uc);
        return;
    }
    
    // Decrement cooldown timer
    if (this->actionCooldownTimer > 0.0f)
    {
        this->actionCooldownTimer -= delta;
    }
    
    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    
    // Dispatch to appropriate mode handler
    switch (this->mode)
    {
        case SupportMode::Normal:
            UpdateNormalMode(uc, toPlayer);
            break;
        case SupportMode::Heal:
            UpdateHealMode(uc);
            break;
        case SupportMode::Buff:
            UpdateBuffMode(uc);
            break;
    }
}

void SupportEnemy::DrawGlowEffect(const Vector3 &pos, Color color, float intensity) const
{
    // Draw expanding sphere glow
    for (int i = 0; i < 3; i++)
    {
        float radius = 0.5f + (i * 0.3f) + (intensity * 0.5f);
        float alpha = (1.0f - i * 0.3f) * intensity;
        DrawSphere(pos, radius, ColorAlpha(color, alpha * 0.3f));
    }
}

void SupportEnemy::Draw() const
{
    // Draw base enemy
    Enemy::Draw();
}

// ---------------------------- VanguardEnemy ----------------------------

Vector3 VanguardEnemy::CalculateBackstabPosition(UpdateContext &uc)
{
    // Position slightly behind the player
    const Camera &cam = uc.player->getCamera();
    Vector3 dir = Vector3Subtract(cam.target, cam.position);
    dir.y = 0.0f;
    if (Vector3LengthSqr(dir) < 0.0001f)
        dir = {0.0f, 0.0f, 1.0f};
    dir = Vector3Normalize(dir);
    Vector3 pos = Vector3Add(cam.position, Vector3Scale(dir, -4.0f));
    pos.y = this->position.y;
    return pos;
}

bool VanguardEnemy::CheckStabHit(UpdateContext &uc)
{
    // Piston Thrust: Use invisible box collision for consistent hit detection
    Vector3 forward = this->stabDirection;
    forward.y = 0.0f;
    if (Vector3LengthSqr(forward) < 0.0001f) forward = {0, 0, 1};
    forward = Vector3Normalize(forward);
    
    // Target player's center for stab collision
    Vector3 playerPos = uc.player->pos();
    
    // Extend hitbox to cover entire spear length plus extra reach
    // Center the box from enemy position extended forward to cover full spear + reach
    float totalReach = this->stabWeaponLength + 2.0f;  // Extra 2 units beyond spear tip
    Vector3 boxCenter = Vector3Add(this->position, Vector3Scale(forward, totalReach * 0.5f));
    boxCenter.y = playerPos.y;  // Align with player center height
    
    // Very wide box to catch the player anywhere along the spear: 4 units wide, 4 units tall (full vertical space), covers entire spear + reach
    float halfWidth = 2.0f;   // Wide to catch side movement
    float halfHeight = 2.0f;  // Tall to cover full vertical space
    float halfLength = totalReach * 0.5f;  // Covers entire spear length
    
    // Check if player OBB intersects with stab box
    Vector3 toPlayer = Vector3Subtract(playerPos, boxCenter);
    
    // Transform to local space (box aligned with forward direction)
    Vector3 right = Vector3Normalize(Vector3{forward.z, 0, -forward.x});
    float localX = fabsf(Vector3DotProduct(toPlayer, right));
    float localY = fabsf(toPlayer.y);
    float localZ = fabsf(Vector3DotProduct(toPlayer, forward));
    
    if (localX <= halfWidth && localY <= halfHeight && localZ <= halfLength)
    {
        // Create collision result at player position
        CollisionResult c;
        c.collided = true;
        c.penetration = 0.5f;
        c.normal = Vector3Scale(forward, -1.0f);  // Normal points away from enemy
        c.with = nullptr;
        
        DamageResult dmg(this->stabDamage, c);
        uc.player->damage(dmg);
        
        // Strong forward knockback
        uc.player->applyKnockback(Vector3Scale(forward, 10.0f), 0.3f, 3.0f);
        
        // Yellow flash at impact point
        if (uc.scene)
        {
            uc.scene->particles.spawnExplosion(playerPos, 12, YELLOW, 0.2f, 4.0f, 0.6f);
        }
        return true;
    }
    return false;
}

bool VanguardEnemy::CheckSlashHit(UpdateContext &uc)
{
    // Crescent Sweep: Use invisible box collision around enemy for consistent hits during dash
    Vector3 center = this->position;
    Vector3 forward = this->getFacingDirection();
    forward.y = 0.0f;
    if (Vector3LengthSqr(forward) < 0.0001f) forward = {0, 0, 1};
    forward = Vector3Normalize(forward);
    
    // Create large box in front of enemy for sweep hitbox, covering full vertical space
    Vector3 boxCenter = Vector3Add(center, Vector3Scale(forward, this->slashRange * 1.5f));
    Vector3 playerPos = uc.player->pos();
    boxCenter.y = playerPos.y;  // Align with player center for full vertical coverage
    
    // Very large box to catch player during dash: 8 units wide (full 180° coverage), 4 units tall (full vertical space), 3x slash range
    float halfWidth = 4.0f;   // Wide arc coverage
    float halfHeight = 2.0f;  // Tall to cover full vertical space below spear
    float halfLength = this->slashRange * 1.5f;
    
    // Check if player is in the box
    Vector3 toPlayer = Vector3Subtract(playerPos, boxCenter);
    
    // Transform to local space
    Vector3 right = Vector3Normalize(Vector3{forward.z, 0, -forward.x});
    float localX = fabsf(Vector3DotProduct(toPlayer, right));
    float localY = fabsf(toPlayer.y);
    float localZ = fabsf(Vector3DotProduct(toPlayer, forward));
    
    if (localX <= halfWidth && localY <= halfHeight && localZ <= halfLength)
    {
        // Create collision result at player position
        Vector3 knockDir = Vector3Normalize(Vector3Subtract(playerPos, center));
        CollisionResult c;
        c.collided = true;
        c.penetration = 0.5f;
        c.normal = Vector3Scale(knockDir, -1.0f);  // Normal points away from enemy
        c.with = nullptr;
        
        DamageResult dmg(this->slashDamage, c);
        uc.player->damage(dmg);
        
        // Strong knockback perpendicular to sweep
        uc.player->applyKnockback(Vector3Scale(knockDir, 12.0f), 0.35f, 4.0f);
        
        // Orange arc visual effect
        if (uc.scene)
        {
            uc.scene->particles.spawnExplosion(playerPos, 18, ORANGE, 0.25f, 5.0f, 0.8f);
        }
        return true;
    }
    return false;
}

void VanguardEnemy::HandleGroundCombo(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->stateTimer -= delta;
    
    if (this->comboStage == 1)
    {
        // STAGE 1: Piston Thrust (Lunge)
        float totalStabTime = this->stabWindupTime + this->stabActiveTime + this->stabRecoveryTime;
        float elapsed = totalStabTime - this->stateTimer;
        
        if (elapsed < this->stabWindupTime)
        {
            // Wind-up: Slowly pull spear back for charge up (smooth ease-in)
            float windupProgress = elapsed / this->stabWindupTime;
            // Ease-in: slow at start, faster at end
            this->spearRetractAmount = windupProgress * windupProgress;  // Quadratic ease-in
            this->spearThrustAmount = 0.0f;
            this->spearSwingAngle = 0.0f;
            
            // Yellow flash telegraph at spear tip
            if (elapsed < 0.05f && uc.scene)
            {
                Vector3 tipPos = Vector3Add(this->position, Vector3Scale(this->stabDirection, 2.0f));
                uc.scene->particles.spawnExplosion(tipPos, 8, YELLOW, 0.15f, 3.0f, 0.4f);
            }
            
            // Freeze in place during windup
            this->velocity = {0, 0, 0};
        }
        else if (elapsed < this->stabWindupTime + this->stabActiveTime)
        {
            // Attack: Fast snap forward (instant at start of attack phase)
            float attackProgress = (elapsed - this->stabWindupTime) / this->stabActiveTime;
            // Quick snap: cubic ease-out for speed
            float snapCurve = 1.0f - powf(1.0f - attackProgress, 3.0f);
            this->spearRetractAmount = fmaxf(0.0f, 1.0f - snapCurve * 5.0f);  // Retract disappears instantly
            this->spearThrustAmount = snapCurve;  // Thrust snaps to full extension quickly
            
            if (!this->comboHitPlayer)
            {
                // Apply forward lunge velocity (sliding motion)
                Vector3 lungeVel = Vector3Scale(this->stabDirection, this->stabLungeForce);
                this->velocity.x = lungeVel.x;
                this->velocity.z = lungeVel.z;
                
                // Check for hit
                if (CheckStabHit(uc))
                {
                    this->comboHitPlayer = true;
                }
                
                // Particle trail along spear path
                if (uc.scene && (int)(elapsed * 60) % 3 == 0)  // Every ~3 frames
                {
                    Vector3 spearTipPos = Vector3Add(this->position, Vector3Scale(this->stabDirection, 2.0f + this->spearThrustAmount * 1.5f));
                    uc.scene->particles.spawnExplosion(spearTipPos, 4, YELLOW, 0.1f, 2.0f, 0.3f);
                }
            }
        }
        else
        {
            // Recovery: Hold spear extended for visual feedback, then slowly retract
            float recoveryProgress = (elapsed - this->stabWindupTime - this->stabActiveTime) / this->stabRecoveryTime;
            if (recoveryProgress < 0.5f)
            {
                // Hold spear fully extended for first half of recovery
                this->spearRetractAmount = 0.0f;
                this->spearThrustAmount = 1.0f;
            }
            else
            {
                // Slowly retract in second half
                float retractProgress = (recoveryProgress - 0.5f) * 2.0f;  // 0 to 1
                this->spearRetractAmount = 0.0f;
                this->spearThrustAmount = 1.0f - retractProgress;
            }
            this->velocity.x *= 0.85f;
            this->velocity.z *= 0.85f;
        }
        
        // Apply ground physics
        Enemy::MovementSettings ms;
        ms.lockToGround = true;
        ms.maxSpeed = 20.0f;  // Allow fast lunge
        ms.maxAccel = 200.0f;
        ms.decelGround = FRICTION * 0.5f;  // Reduced friction for sliding
        ms.decelAir = AIR_DRAG;
        ms.facingHint = this->stabDirection;
        this->UpdateCommonBehavior(uc, {0, 0, 0}, delta, ms);
        
        // Transition to slash
        if (this->stateTimer <= 0.0f)
        {
            this->comboStage = 2;
            this->stateTimer = this->slashWindupTime + this->slashActiveTime + this->slashRecoveryTime;
            this->comboHitPlayer = false;
            this->velocity = {0, 0, 0};
        }
    }
    else if (this->comboStage == 2)
    {
        // STAGE 2: Crescent Sweep (Slash with Dash)
        float totalSlashTime = this->slashWindupTime + this->slashActiveTime + this->slashRecoveryTime;
        float elapsed = totalSlashTime - this->stateTimer;
        
        if (elapsed < this->slashWindupTime)
        {
            // Wind-up: Turn and drag spearhead, prepare swing
            this->spearSwingAngle = 0.0f;
            this->spearThrustAmount = 0.0f;
            this->spearRetractAmount = 0.0f;
            
            // Orange glow telegraph
            if (elapsed < 0.05f && uc.scene)
            {
                Vector3 tipPos = Vector3Add(this->position, Vector3Scale(this->getFacingDirection(), 1.5f));
                uc.scene->particles.spawnExplosion(tipPos, 10, ORANGE, 0.15f, 3.5f, 0.5f);
            }
            
            // Stand still
            this->velocity = {0, 0, 0};
        }
        else if (elapsed < this->slashWindupTime + this->slashActiveTime)
        {
            // Attack: Wide sweep with aggressive dash toward player
            float attackProgress = (elapsed - this->slashWindupTime) / this->slashActiveTime;
            
            // Smooth swing spear in an arc from -90 (left) to +90 (right) degrees
            this->spearSwingAngle = this->spearSwingStartAngle + (attackProgress * this->slashArcDegrees);
            
            // Dash toward player during slash with aggressive acceleration (mimicking dive)
            Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
            toPlayer.y = 0.0f;
            if (Vector3LengthSqr(toPlayer) > 0.001f)
                toPlayer = Vector3Normalize(toPlayer);
            else
                toPlayer = this->getFacingDirection();
            
            // Apply very aggressive dash velocity for gap closing with longer dash
            float dashSpeed = 50.0f + (attackProgress * 60.0f);  // 50-110 speed (increased)
            this->velocity.x = toPlayer.x * dashSpeed;
            this->velocity.z = toPlayer.z * dashSpeed;
            
            if (!this->comboHitPlayer)
            {
                // Check for slash weapon hit
                if (CheckSlashHit(uc))
                {
                    this->comboHitPlayer = true;
                }
                
                // Also check for body collision during dash (enemy body is dangerous during slash dash)
                CollisionResult bodyHit = GetCollisionOBBvsOBB(&this->obj().obb, &uc.player->obj().obb);
                if (bodyHit.collided)
                {
                    DamageResult dmg(this->slashDamage * 0.8f, bodyHit);  // Slightly less damage than weapon hit
                    uc.player->damage(dmg);
                    
                    Vector3 dashKnockDir = Vector3Normalize(Vector3Subtract(uc.player->pos(), this->position));
                    uc.player->applyKnockback(Vector3Scale(dashKnockDir, 10.0f), 0.3f, 3.5f);
                    
                    if (uc.scene)
                    {
                        uc.scene->particles.spawnExplosion(uc.player->pos(), 15, ORANGE, 0.2f, 4.5f, 0.7f);
                    }
                    this->comboHitPlayer = true;
                }
                
                // White particle trail showing full attack arc range
                if (uc.scene)
                {
                    // Calculate current spear tip position based on swing angle
                    Vector3 enemyForward = this->getFacingDirection();
                    enemyForward.y = 0.0f;
                    if (Vector3LengthSqr(enemyForward) < 0.0001f) enemyForward = {0, 0, 1};
                    enemyForward = Vector3Normalize(enemyForward);
                    
                    Vector3 rightDir = {enemyForward.z, 0, -enemyForward.x};
                    
                    // Spawn particles along the full arc to show attack coverage
                    // Use slash range (doubled in CheckSlashHit) to show actual hit zone
                    float effectiveRange = this->slashRange * 2.0f;
                    
                    // Sample multiple points along the arc from center to edge
                    for (int radiusStep = 0; radiusStep < 3; radiusStep++)
                    {
                        float radiusFactor = 0.3f + (radiusStep * 0.35f);  // 0.3, 0.65, 1.0 of range
                        float currentRadius = effectiveRange * radiusFactor;
                        
                        // Sample several angles along the current swing
                        for (int arcStep = 0; arcStep < 5; arcStep++)
                        {
                            // Trail spans from start of swing to current position
                            float angleOffset = arcStep * 12.0f;  // Span 48 degrees of recent arc
                            float arcSampleAngle = this->spearSwingAngle - angleOffset;
                            
                            // Skip if before swing start
                            if (arcSampleAngle < this->spearSwingStartAngle) continue;
                            
                            float currentAngleRad = arcSampleAngle * DEG2RAD;
                            
                            Vector3 tipOffset;
                            tipOffset.x = -sinf(currentAngleRad) * currentRadius;  // Left-right
                            tipOffset.z = cosf(currentAngleRad) * currentRadius;   // Forward-back
                            tipOffset.y = 0.5f;
                            
                            Vector3 arcPos = this->position;
                            arcPos.x += enemyForward.x * tipOffset.z + rightDir.x * tipOffset.x;
                            arcPos.z += enemyForward.z * tipOffset.z + rightDir.z * tipOffset.x;
                            arcPos.y += tipOffset.y;
                            
                            // Spawn white trail particles with fade
                            float alpha = 0.7f - (arcStep * 0.1f) - (radiusStep * 0.15f);
                            Color trailColor = ColorAlpha(WHITE, (unsigned char)(alpha * 255));
                            uc.scene->particles.spawnExplosion(arcPos, 1, trailColor, 0.18f, 1.0f, 0.35f);
                        }
                    }
                }
            }
        }
        else
        {
            // Long recovery: Hold spear extended at end of arc (punish window)
            float recoveryProgress = (elapsed - this->slashWindupTime - this->slashActiveTime) / this->slashRecoveryTime;
            if (recoveryProgress < 0.6f)
            {
                // Hold at full extension for first 60% of recovery
                this->spearSwingAngle = this->spearSwingStartAngle + this->slashArcDegrees;
            }
            else
            {
                // Slowly return to neutral in last 40%
                float returnProgress = (recoveryProgress - 0.6f) / 0.4f;
                float finalAngle = this->spearSwingStartAngle + this->slashArcDegrees;
                this->spearSwingAngle = finalAngle * (1.0f - returnProgress);
            }
            this->velocity = {0, 0, 0};
        }
        
        // Stay grounded during slash
        Enemy::MovementSettings ms;
        ms.lockToGround = true;
        ms.maxSpeed = 15.0f;  // Allow dash
        ms.maxAccel = MAX_ACCEL * 1.5f;
        ms.decelGround = FRICTION;
        ms.decelAir = AIR_DRAG;
        ms.facingHint = this->getFacingDirection();
        this->UpdateCommonBehavior(uc, {0, 0, 0}, delta, ms);
        
        // End combo
        if (this->stateTimer <= 0.0f)
        {
            this->state = VanguardState::Chasing;
            this->comboStage = 0;
            this->comboHitPlayer = false;
            this->spearRetractAmount = 0.0f;
            this->spearThrustAmount = 0.0f;
            this->spearSwingAngle = 0.0f;
        }
        }
    
        this->updateElectrocute(delta);
        this->UpdateDialog(uc);}

void VanguardEnemy::DecideAction(UpdateContext &uc, float distanceToPlayer)
{
    // Weighted RNG system based on distance zones (from design doc)
    
    // Zone A: The "Kill Box" (Distance < 3.0 units)
    if (distanceToPlayer < 10.0f)
    {
        // 90% Ground Combo, 10% Panic Dive
        float roll = (float)GetRandomValue(0, 100) / 100.0f;
        if (roll < 0.9f)
        {
            // Start ground combo with Piston Thrust
            this->state = VanguardState::GroundComboStab;
            this->comboStage = 1;
            this->stateTimer = this->stabWindupTime + this->stabActiveTime + this->stabRecoveryTime;
            this->comboHitPlayer = false;
            
            // Store stab direction
            Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
            toPlayer.y = 0.0f;
            if (Vector3LengthSqr(toPlayer) > 0.001f)
                this->stabDirection = Vector3Normalize(toPlayer);
            else
                this->stabDirection = this->getFacingDirection();
        }
        else
        {
            // Panic dive to escape
            this->state = VanguardState::AerialAscend;
            this->stateTimer = this->diveAscendTime;
            this->velocity.y = this->diveAscendInitialVelocity;
            this->diveCooldownTimer = this->diveCooldownDuration;
        }
    }
    // Zone B: The "Skirmish" (Distance 10.0 - 30.0 units)
    else if (distanceToPlayer >= 10.0f && distanceToPlayer <= 30.0f)
    {
        // 30% Aerial Dive, 40% Run Forward
        float roll = (float)GetRandomValue(0, 100) / 100.0f;
        if (roll < 0.3f && this->diveCooldownTimer <= 0.0f)
        {
            // Dive to close gap instantly
            this->state = VanguardState::AerialAscend;
            this->stateTimer = this->diveAscendTime;
            this->velocity.y = this->diveAscendInitialVelocity;
            this->diveCooldownTimer = this->diveCooldownDuration;
        }
        // else: Stay in Chasing state (run forward)
    }
    // Zone C: The "Sniper" (Distance > 10.0 units)
    else
    {
        // 50% chance to dive if ready (punish running)
        float roll = (float)GetRandomValue(0, 100) / 100.0f;
        if (roll < 0.5f && this->diveCooldownTimer <= 0.0f)
        {
            this->state = VanguardState::AerialAscend;
            this->stateTimer = this->diveAscendTime;
            this->velocity.y = this->diveAscendInitialVelocity;
            this->diveCooldownTimer = this->diveCooldownDuration;
        }
        // else: Stay in Chasing state (run forward)
    }
}

void VanguardEnemy::HandleAerialDive(UpdateContext &uc)
{
    float delta = GetFrameTime();
    
    // Update shockwave if active
    if (this->shockwaveActive)
    {
        this->shockwaveRadius += this->shockwaveExpandSpeed * delta;
        
        // Check for player hit by shockwave (only once per shockwave)
        if (!this->shockwaveHitPlayer && this->shockwaveRadius > 0.5f)
        {
            Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->shockwaveCenter);
            toPlayer.y = 0.0f;  // Ignore height for shockwave radius check
            float distToPlayer = Vector3Length(toPlayer);
            
            // Shockwave hits if player is within current radius and wasn't hit by this shockwave yet
            if (distToPlayer <= this->shockwaveRadius)
            {
                // Only damage if player is on ground or close to ground (can jump over)
                // Player can avoid by being in the air
                if (uc.player->isGrounded() || uc.player->pos().y <= this->shockwaveCenter.y + 1.5f)
                {
                    CollisionResult c;
                    c.collided = true;
                    c.penetration = 1.0f;
                    c.normal = Vector3Normalize(toPlayer);
                    if (Vector3LengthSqr(c.normal) < 0.0001f) c.normal = {0, 0, -1};
                    c.with = nullptr;
                    
                    DamageResult dmg(this->shockwaveDamage, c);
                    uc.player->damage(dmg);
                    
                    // Radial knockback away from center
                    Vector3 knockDir = Vector3Normalize(toPlayer);
                    uc.player->applyKnockback(Vector3Scale(knockDir, 12.0f), 0.35f, 5.0f);
                    
                    this->shockwaveHitPlayer = true;
                }
            }
        }
        
        // Stop shockwave when it reaches max radius
        if (this->shockwaveRadius >= this->shockwaveMaxRadius)
        {
            this->shockwaveActive = false;
        }
    }
    if (this->state == VanguardState::AerialAscend)
    {
        // Fast ascent eased to slow by strong gravity
        this->velocity.y = fmaxf(this->velocity.y, this->diveAscendInitialVelocity);
        this->stateTimer -= delta;
        this->velocity.y -= this->diveGravityDuringAscent * delta;
        this->position.y += this->velocity.y * delta;
        this->o.pos = this->position; // Update object position
        this->o.UpdateOBB(); // Update collision bounds
        if (this->stateTimer <= 0.0f)
        {
            this->state = VanguardState::AerialHover;
            this->stateTimer = this->diveHangTime; // 1.5 seconds
            // Lock target to player camera position
            const Camera &cam = uc.player->getCamera();
            Vector3 forward = Vector3Subtract(cam.target, cam.position);
            if (Vector3LengthSqr(forward) < 0.0001f)
                forward = {0.0f, 0.0f, 1.0f};
            forward = Vector3Normalize(forward);
            this->diveTargetPos = Vector3Add(cam.position, Vector3Scale(forward, 4.0f));
            this->diveTargetPos.y = 0.0f;
            this->diveCurrentSpeed = this->diveInitialSpeed;
            // Telegraph particles
            if (uc.scene)
            {
                uc.scene->particles.spawnRing(this->position, 2.5f, 14, RED, 0.6f, true);
            }
        }
    }
    else if (this->state == VanguardState::AerialHover)
    {
        // Hover and continuously track player camera
        this->stateTimer -= delta;
        this->rotationTowardsPlayer = Lerp(this->rotationTowardsPlayer, 1.0f, delta * 3.0f);
        
        // Continuously update facing direction to track camera in real-time
        const Camera &cam = uc.player->getCamera();
        Vector3 camPos = cam.position;
        Vector3 toCam = Vector3Subtract(camPos, this->position);
        toCam.y = 0.0f;
        if (Vector3LengthSqr(toCam) > 0.001f)
        {
            this->o.setRotationFromForward(Vector3Normalize(toCam));
        }
        
        if (this->stateTimer <= 0.0f)
        {
            this->state = VanguardState::AerialDive;
            // Set dive direction from current position directly to player's position (including Y)
            Vector3 playerPos = uc.player->pos();
            Vector3 diveDirToPlayer = Vector3Subtract(playerPos, this->position);
            
            // Keep the full 3D direction to dive directly at the player
            if (Vector3LengthSqr(diveDirToPlayer) < 0.001f)
                diveDirToPlayer = {0.0f, -1.0f, 1.0f};
            diveDirToPlayer = Vector3Normalize(diveDirToPlayer);
            
            this->diveCurrentSpeed = this->diveInitialSpeed;
            this->velocity = Vector3Scale(diveDirToPlayer, this->diveCurrentSpeed);
        }
    }
    else if (this->state == VanguardState::AerialDive)
    {
        // Very fast dive with aggressive acceleration toward player
        float cur = Vector3Length(this->velocity);
        cur = fmaxf(cur, 0.0001f);
        float add = this->diveAcceleration * delta;
        this->diveCurrentSpeed = fminf(this->diveCurrentSpeed + add, this->diveMaxSpeed);
        
        // Normalize current velocity direction and scale by new speed
        Vector3 dir = Vector3Normalize(this->velocity);
        this->velocity = Vector3Scale(dir, this->diveCurrentSpeed);
        
        // Update position with room bounds checking
        Vector3 newPosition = Vector3Add(this->position, Vector3Scale(this->velocity, delta));
        
        // Check room bounds to prevent diving out
        if (uc.scene)
        {
            Room *r = uc.scene->GetRoomContainingPosition(this->position);
            if (r)
            {
                BoundingBox bounds = r->GetBounds();
                // Clamp position to room bounds with small margin
                float margin = 2.0f;
                if (newPosition.x < bounds.min.x + margin || newPosition.x > bounds.max.x - margin ||
                    newPosition.z < bounds.min.z + margin || newPosition.z > bounds.max.z - margin)
                {
                    // Hit wall - stop dive and land
                    newPosition.x = Clamp(newPosition.x, bounds.min.x + margin, bounds.max.x - margin);
                    newPosition.z = Clamp(newPosition.z, bounds.min.z + margin, bounds.max.z - margin);
                    newPosition.y = bounds.min.y; // Force to ground
                    this->position = newPosition;
                    this->velocity = {0.0f, 0.0f, 0.0f};
                    this->state = VanguardState::AerialLanding;
                    this->stateTimer = this->diveLandingRecoveryTime;
                    this->diveCurrentSpeed = 0.0f;
                    this->visualScale = {1.6f, 0.6f, 1.6f};
                    this->o.pos = this->position;
                    this->o.UpdateOBB();
                    
                    // Start shockwave on wall impact landing
                    this->shockwaveActive = true;
                    this->shockwaveRadius = 0.0f;
                    this->shockwaveCenter = this->position;
                    this->shockwaveHitPlayer = false;
                    
                    // Wall impact particles
                    if (uc.scene)
                    {
                        uc.scene->particles.spawnExplosion(this->position, 24, ORANGE, 0.3f, 6.0f, 1.0f);
                    }
                    return; // Exit early
                }
            }
        }
        
        this->position = newPosition;
        this->o.pos = this->position; // Update object position
        this->o.UpdateOBB(); // Update collision bounds

        // Check for player collision first (before ground impact)
        bool hitPlayer = false;
        if (CheckCollisionSphereVsOBB(this->position, 2.5f, &uc.player->obj().obb))
        {
            hitPlayer = true;
            // Apply damage and knockback to player
            CollisionResult c = Object::collided(this->obj(), uc.player->obj());
            DamageResult dmg(this->diveDamage, c);
            uc.player->damage(dmg);
            // Knockback direction: from enemy to player
            Vector3 knock = Vector3Normalize(Vector3Subtract(uc.player->pos(), this->position));
            uc.player->applyKnockback(Vector3Scale(knock, 14.0f), 0.45f, 6.0f);
        }

        // Check for ground impact
        float floorY = 0.0f;
        if (uc.scene)
        {
            Room *r = uc.scene->GetRoomContainingPosition(this->position);
            if (r)
                floorY = r->GetBounds().min.y;
        }

        bool impacted = hitPlayer;  // End dive if hit player
        if (this->position.y <= floorY + 0.5f)
        {
            impacted = true;
            this->position.y = floorY;
        }

        if (impacted)
        {
            // Impact particles: play on ground impact or when hitting the player
            if (uc.scene && (hitPlayer || this->position.y <= floorY + 0.5f))
            {
                // Use the same landing/explosion visual whether we hit the player mid-air or hit the ground
                uc.scene->particles.spawnExplosion(this->position, 48, RED, 0.4f, 8.0f, 1.0f);
                uc.scene->particles.spawnRing(this->position, 4.0f, 28, ColorAlpha(ORANGE, 220), 3.2f, true);
                // Strong screen shake on impact (player hit or ground)
                uc.player->addCameraShake(3.0f, 0.8f);
            }

            // Start shockwave on landing
            this->shockwaveActive = true;
            this->shockwaveRadius = 0.0f;
            this->shockwaveCenter = this->position;
            this->shockwaveHitPlayer = false;

            // Squash visual and transition to landing
            this->visualScale = {1.6f, 0.6f, 1.6f};
            this->state = VanguardState::AerialLanding;
            this->stateTimer = this->diveLandingRecoveryTime;
            this->velocity = {0.0f, 0.0f, 0.0f};
            this->diveCurrentSpeed = 0.0f;
            
            // Sync position after setting to ground
            this->o.pos = this->position;
            this->o.UpdateOBB();
        }
    }
    else if (this->state == VanguardState::AerialLanding)
    {
        this->stateTimer -= delta;
        
        // Continue expanding shockwave during landing recovery
        if (this->shockwaveActive)
        {
            this->shockwaveRadius += this->shockwaveExpandSpeed * delta;
            
            // Check for player hit by shockwave (only once per shockwave)
            if (!this->shockwaveHitPlayer && this->shockwaveRadius > 0.5f)
            {
                Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->shockwaveCenter);
                toPlayer.y = 0.0f;  // Ignore height for shockwave radius check
                float distToPlayer = Vector3Length(toPlayer);
                
                // Shockwave hits if player is within current radius
                if (distToPlayer <= this->shockwaveRadius)
                {
                    // Only damage if player is on ground or close to ground (can jump over)
                    if (uc.player->isGrounded() || uc.player->pos().y <= this->shockwaveCenter.y + 1.5f)
                    {
                        CollisionResult c;
                        c.collided = true;
                        c.penetration = 1.0f;
                        c.normal = Vector3Normalize(toPlayer);
                        if (Vector3LengthSqr(c.normal) < 0.0001f) c.normal = {0, 0, -1};
                        c.with = nullptr;
                        
                        DamageResult dmg(this->shockwaveDamage, c);
                        uc.player->damage(dmg);
                        
                        Vector3 knockDir = Vector3Normalize(toPlayer);
                        uc.player->applyKnockback(Vector3Scale(knockDir, 12.0f), 0.35f, 5.0f);
                        
                        this->shockwaveHitPlayer = true;
                    }
                }
            }
            
            if (this->shockwaveRadius >= this->shockwaveMaxRadius)
            {
                this->shockwaveActive = false;
            }
        }
        
        // Ease visual back to normal
        this->visualScale.x = Lerp(this->visualScale.x, 1.0f, delta * 8.0f);
        this->visualScale.y = Lerp(this->visualScale.y, 1.0f, delta * 8.0f);
        this->visualScale.z = Lerp(this->visualScale.z, 1.0f, delta * 8.0f);
        if (this->stateTimer <= 0.0f)
        {
            this->state = VanguardState::Chasing;
        }
    }
    
    this->updateElectrocute(delta);
    // Update health bar position for all aerial states
    this->UpdateDialog(uc);
}

// Static model initialization
Model VanguardEnemy::sharedSpearModel = {0};
bool VanguardEnemy::spearModelLoaded = false;

void VanguardEnemy::LoadSharedResources()
{
    if (!spearModelLoaded)
    {
        sharedSpearModel = LoadModel("spear.glb");
        spearModelLoaded = true;
    }
}

void VanguardEnemy::UnloadSharedResources()
{
    if (spearModelLoaded)
    {
        UnloadModel(sharedSpearModel);
        spearModelLoaded = false;
    }
}

void VanguardEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->tickStatusTimers(delta);
    bool isStunned = this->updateStun(uc);

    if (isStunned || this->isMovementDisabled())
    {
        // If stunned mid-air, should fall. If in a combo, should stop.
        if (this->state != VanguardState::Chasing)
        {
            this->state = VanguardState::Chasing; // Revert to a neutral state
            this->comboStage = 0;
        }
        Enemy::MovementSettings ms;
        ms.maxSpeed = 0.0f;
        ms.maxAccel = 0.0f;
        this->UpdateCommonBehavior(uc, Vector3Zero(), delta, ms);
        this->updateElectrocute(delta);
        this->UpdateDialog(uc);
        return;
    }

    // --- Not stunned from here on ---

    // Cache camera info for use during Draw (Draw is const and has no UpdateContext)
    const Camera &cam = uc.player->getCamera();
    this->cachedCameraPos = cam.position;
    // Yaw: atan2(x, z) from enemy to camera
    float camYawRad = atan2f(cam.position.x - this->position.x, cam.position.z - this->position.z);
    this->cachedCameraYawDeg = camYawRad * RAD2DEG;
    // Pitch: vertical angle from enemy to camera
    Vector3 toCam = Vector3Subtract(cam.position, this->position);
    float horiz = sqrtf(toCam.x * toCam.x + toCam.z * toCam.z);
    this->cachedCameraPitchDeg = atan2f(toCam.y, horiz) * RAD2DEG;
    this->diveCooldownTimer = fmaxf(0.0f, this->diveCooldownTimer - delta);
    this->decisionCooldownTimer = fmaxf(0.0f, this->decisionCooldownTimer - delta);

    if (this->state == VanguardState::Chasing)
    {
        Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
        toPlayer.y = 0.0f;
        float dist = Vector3Length(toPlayer);

        // Use decision logic periodically (not every frame)
        if (this->decisionCooldownTimer <= 0.0f)
        {
            DecideAction(uc, dist);
            this->decisionCooldownTimer = this->decisionCooldownDuration;
        }
        
        // If still chasing (no action triggered), move toward player
        if (this->state == VanguardState::Chasing)
        {
            Vector3 desired = Vector3Zero();
            if (dist > 1.5f)
                desired = Vector3Normalize(toPlayer);

            Enemy::MovementSettings ms;
            ms.lockToGround = true;
            ms.maxSpeed = this->chaseSpeed;
            ms.maxAccel = MAX_ACCEL * 2.0f;
            ms.decelGround = FRICTION;
            ms.decelAir = AIR_DRAG;
            ms.facingHint = toPlayer;
            this->UpdateCommonBehavior(uc, desired, delta, ms);
            this->updateElectrocute(delta);
            this->UpdateDialog(uc);
        }
    }
    else if (this->state == VanguardState::GroundComboStab || this->state == VanguardState::GroundComboSlash)
    {
        HandleGroundCombo(uc);
    }
    else if (this->state == VanguardState::AerialAscend || this->state == VanguardState::AerialHover || this->state == VanguardState::AerialDive || this->state == VanguardState::AerialLanding)
    {
        HandleAerialDive(uc);
    }
}

void VanguardEnemy::Draw() const
{
    // Draw the spear weapon if loaded
    if (spearModelLoaded)
    {
        // Get enemy's current rotation
        Vector3 enemyForward = this->getFacingDirection();
        enemyForward.y = 0.0f;
        if (Vector3LengthSqr(enemyForward) < 0.0001f)
            enemyForward = {0, 0, 1};
        enemyForward = Vector3Normalize(enemyForward);
        
        // Calculate rotation angle around Y axis
        float angleRad = atan2f(enemyForward.x, enemyForward.z);
        float angleDeg = angleRad * RAD2DEG;
        
        // Base position at enemy's side (lower position)
        Vector3 rightDir = {enemyForward.z, 0, -enemyForward.x};  // Perpendicular to forward
        Vector3 spearPos = Vector3Add(this->position, Vector3Scale(rightDir, this->spearOffset.x));
        spearPos.y += this->spearOffset.y + 0.4f;  // Lower spear (was 0.8f)
        
        // Use cached camera info (updated during UpdateBody)
        float camAngleDeg = this->cachedCameraYawDeg;
        float verticalAngle = this->cachedCameraPitchDeg;

        // Current rotation for spear (default to facing direction)
        float currentYRotation = angleDeg;

        // Apply thrust animation (forward along facing direction toward camera)
        if (this->spearThrustAmount > 0.0f)
        {
            // During thrust, point at camera
            currentYRotation = camAngleDeg;
            spearPos = Vector3Add(spearPos, Vector3Scale(enemyForward, this->spearThrustAmount * 4.0f));
        }
        
        // Apply retract animation (backward along facing direction)
        if (this->spearRetractAmount > 0.0f)
        {
            spearPos = Vector3Add(spearPos, Vector3Scale(enemyForward, -this->spearRetractAmount * 4.0f));
        }
        
        float tiltAngle = 0.0f;  // No tilt (issue resolved by model change)
        
        // During stab, point at camera
        if (this->spearThrustAmount > 0.0f || this->spearRetractAmount > 0.0f)
        {
            currentYRotation = camAngleDeg;
            tiltAngle = 0.0f;  // Keep level
        }
        
        // Apply swing animation during slash (rotate through full arc, targeting camera)
        if (this->spearSwingAngle != 0.0f)
        {
            // Spear swings from left (-90) to right (+90), pointing at camera
            currentYRotation = camAngleDeg + this->spearSwingAngle;
            tiltAngle = 0.0f;  // Keep level
            
            // Position spear at swing radius around enemy
            float swingRadius = 2.0f;
            float swingAngleRad = (camAngleDeg + this->spearSwingAngle) * DEG2RAD;
            
            spearPos = this->position;
            spearPos.x += sinf(swingAngleRad) * swingRadius;
            spearPos.z += cosf(swingAngleRad) * swingRadius;
            spearPos.y += 0.5f;  // Lower during slash
        }
        
        // Draw the spear model with proper rotation
        // Model points up by default: +90° X to point forward, then tilt based on camera
        
        // Smooth spear position and rotation to reduce jitter
        float smoothFactor = 0.25f;  // Lower = smoother but more lag
        this->smoothedSpearPos = Vector3Lerp(this->smoothedSpearPos, spearPos, smoothFactor);
        this->smoothedYRotation = Lerp(this->smoothedYRotation, currentYRotation, smoothFactor);
        
        spearPos = this->smoothedSpearPos;
        currentYRotation = this->smoothedYRotation;
        
        // Base rotation: point forward first
        float totalXRotation = 90.0f + tiltAngle;  // Point forward (90) + camera tilt
        
        // Use DrawModelEx with proper rotation sequence
        // We need to rotate in local space first, then apply world rotation
        
        // Create rotation as Euler angles
        Matrix matScale = MatrixScale(this->spearScale, this->spearScale, this->spearScale);
        Matrix matRotateX = MatrixRotateX(totalXRotation * DEG2RAD);  // Tilt down 45°
        Matrix matRotateY = MatrixRotateY(currentYRotation * DEG2RAD);  // Face direction
        Matrix matTranslate = MatrixTranslate(spearPos.x, spearPos.y, spearPos.z);
        
        // Combine: Scale -> RotateX (tilt) -> RotateY (facing) -> Translate
        Matrix transform = matScale;
        transform = MatrixMultiply(transform, matRotateX);
        transform = MatrixMultiply(transform, matRotateY);
        transform = MatrixMultiply(transform, matTranslate);
        
        // Draw using the transform matrix with proper materials
        for (int i = 0; i < sharedSpearModel.meshCount; i++)
        {
            int matIndex = (i < sharedSpearModel.materialCount) ? i : 0;
            DrawMesh(sharedSpearModel.meshes[i], sharedSpearModel.materials[matIndex], transform);
        }
    }
    
    // Draw shockwave as expanding circle of white smoke
    if (this->shockwaveActive && this->shockwaveRadius > 0.0f)
    {
        // Draw expanding ring of white particles
        // The shockwave expands outward in a circular pattern
        int particleCount = (int)(this->shockwaveRadius * 4.0f);  // More particles as it expands
        particleCount = Clamp(particleCount, 16, 64);
        
        float ringThickness = 0.8f;  // Width of the ring
        Vector3 ringBasePos = this->shockwaveCenter;
        ringBasePos.y += 0.3f;  // Slightly above ground
        
        // Draw particles in a circle at current shockwave radius
        for (int i = 0; i < particleCount; i++)
        {
            float angle = (i / (float)particleCount) * PI * 2.0f;
            float radius = this->shockwaveRadius;
            
            Vector3 particlePos;
            particlePos.x = ringBasePos.x + cosf(angle) * radius;
            particlePos.z = ringBasePos.z + sinf(angle) * radius;
            particlePos.y = ringBasePos.y;
            
            // Fade out as shockwave expands
            float alphaFade = 1.0f - (this->shockwaveRadius / this->shockwaveMaxRadius);
            alphaFade = fmaxf(0.0f, alphaFade);
            
            Color particleColor = ColorAlpha(WHITE, (unsigned char)(alphaFade * 200.0f));
            DrawSphere(particlePos, 0.15f, particleColor);
        }
    }
}
