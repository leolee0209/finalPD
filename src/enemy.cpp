#include "me.hpp"       // Contains Enemy class definition
#include "dialogBox.hpp"
#include "scene.hpp"    // For Scene class and its methods
#include "raymath.h"    // For Vector3 operations
#include "constant.hpp" // For constants like GRAVITY, FRICTION, AIR_DRAG, MAX_SPEED, MAX_ACCEL
#include <iostream>
#include <cstdio>
#include <cmath> // For sinf, cosf
#include <algorithm>

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
        this->position.y = floory;
        this->velocity.y = 0.0f;
        this->grounded = true;
        this->o.pos = this->position;
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
        this->snapToGroundWithRotation(this->o.getRotation());
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
    Vector3 directionToPlayer = Vector3Subtract(uc.player->pos(), this->position);
    directionToPlayer.y = 0.0f;

    MovementSettings settings;
    settings.maxSpeed = 3.0f;
    settings.facingHint = directionToPlayer;

    this->UpdateCommonBehavior(uc, directionToPlayer, delta, settings);
    // Update health dialog owned by this enemy (position/text/orientation)
    this->UpdateDialog(uc);
}

void ChargingEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    float distanceToPlayer = Vector3Length(toPlayer);

    Vector3 desiredDirection = Vector3Zero();
    float targetSpeed = this->approachSpeed;
    float targetPoseDeg = 0.0f;
    bool usesStateTimer = false;
    bool timerWaitsForPose = false;

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

    MovementSettings settings;
    settings.maxSpeed = targetSpeed;
    settings.facingHint = toPlayer;
    settings.lockToGround = true;
    settings.enableLean = (this->state != ChargeState::Charging);
    settings.enableBobAndSway = (this->state != ChargeState::Charging);

    if (this->state == ChargeState::Charging)
    {
        settings.maxAccel = this->chargeSpeed * 200.0f;
        settings.decelGround = 1.0f;
        settings.decelAir = 1.0f;
        settings.zeroThreshold = 0.0f;
        settings.overrideHorizontalVelocity = true;
        settings.forcedHorizontalVelocity = Vector3Scale(this->chargeDirection, this->chargeSpeed);
    }

    this->UpdateCommonBehavior(uc, desiredDirection, delta, settings);

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

    if (this->state == ChargeState::Charging)
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
        if (this->state == ChargeState::Charging)
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

    if (this->state == ChargeState::Charging && !this->appliedChargeDamage)
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
        this->grounded = false;
    }
    this->knockbackTimer = fmaxf(this->knockbackTimer, durationSeconds);
}

void Enemy::gatherObjects(std::vector<Object *> &out) const
{
    out.push_back(const_cast<Object *>(&this->o));
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

ShooterEnemy::ShooterEnemy()
{
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

void ShooterEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    float distance = Vector3Length(toPlayer);

    Vector3 aimDir = {0.0f, 0.0f, 0.0f};
    bool hasLineOfSight = this->findShotDirection(uc, aimDir);
    bool withinRange = this->isWithinPreferredRange(distance);

    MovementCommand command{};
    if (this->phase == Phase::FindPosition)
    {
        command = this->FindMovement(uc, toPlayer, distance, hasLineOfSight, delta);
        if (withinRange && hasLineOfSight)
        {
            this->phase = Phase::Shooting;
        }
    }
    else
    {
        if (!withinRange || !hasLineOfSight)
        {
            this->phase = Phase::FindPosition;
            command = this->FindMovement(uc, toPlayer, distance, hasLineOfSight, delta);
        }
    }

    MovementSettings settings;
    settings.maxSpeed = command.speed;
    settings.facingHint = toPlayer;
    settings.lockToGround = true;
    settings.enableLean = command.speed > 0.1f;
    settings.enableBobAndSway = command.speed > 0.1f;

    this->UpdateCommonBehavior(uc, command.direction, delta, settings);

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

    this->updateBullets(uc, delta);
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
                return true;
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
