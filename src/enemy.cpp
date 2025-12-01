#include "me.hpp"       // Contains Enemy class definition
#include "scene.hpp"    // For Scene class and its methods
#include "raymath.h"    // For Vector3 operations
#include "constant.hpp" // For constants like GRAVITY, FRICTION, AIR_DRAG, MAX_SPEED, MAX_ACCEL
#include <iostream>
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

ShooterEnemy::ShooterEnemy() = default;

void ShooterEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    float distance = Vector3Length(toPlayer);

    Vector3 previewAimDir = {0};
    bool hasLineOfSight = this->findShotDirection(uc, previewAimDir);

    Vector3 desiredDirection = Vector3Zero();
    float targetSpeed = 0.0f;

    if (distance > this->maxFiringDistance)
    {
        desiredDirection = toPlayer;
        targetSpeed = 6.0f;
        this->losRepositionTimer = 0.0f;
    }
    else if (distance < this->retreatDistance)
    {
        desiredDirection = Vector3Scale(toPlayer, -1.0f);
        targetSpeed = 6.5f;
        this->losRepositionTimer = 0.0f;
    }
    else if (!hasLineOfSight)
    {
        Vector3 lateral = Vector3CrossProduct({0.0f, 1.0f, 0.0f}, toPlayer);
        if (Vector3LengthSqr(lateral) > 0.001f)
        {
            lateral = Vector3Normalize(lateral);
            desiredDirection = Vector3Scale(lateral, static_cast<float>(this->strafeDirection));
            targetSpeed = 4.0f;
        }

        this->losRepositionTimer += delta;
        if (this->losRepositionTimer >= 1.2f)
        {
            this->strafeDirection *= -1;
            this->losRepositionTimer = 0.0f;
        }
    }
    else
    {
        this->losRepositionTimer = 0.0f;
    }

    MovementSettings settings;
    settings.maxSpeed = targetSpeed;
    settings.facingHint = toPlayer;
    settings.lockToGround = true;
    settings.enableLean = true;
    settings.enableBobAndSway = targetSpeed > 0.1f;

    this->UpdateCommonBehavior(uc, desiredDirection, delta, settings);

    this->fireCooldown -= delta;
    Vector3 aimDir = {0};
    bool canShoot = this->findShotDirection(uc, aimDir);
    Vector3 muzzle = this->position;
    muzzle.y += this->muzzleHeight;

    if (this->fireCooldown <= 0.0f && canShoot && (int)this->bullets.size() < this->maxActiveBullets)
    {
        this->spawnBullet(muzzle, aimDir);
        this->fireCooldown = this->fireInterval;
    }

    this->updateBullets(uc, delta);
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

    Vector3 dir = Vector3Scale(toPlayer, 1.0f / distance);
    Vector3 endPoint = targetPoint;

    if (!this->hasLineOfFire(muzzle, endPoint, uc))
        return false;

    outDir = dir;
    return true;
}

bool ShooterEnemy::hasLineOfFire(const Vector3 &start, const Vector3 &end, UpdateContext &uc) const
{
    auto staticObjects = uc.scene->getStaticObjects();
    for (auto *obj : staticObjects)
    {
        if (!obj)
            continue;
        if (CheckLineSegmentVsOBB(start, end, this->bulletRadius, &obj->obb))
        {
            return false;
        }
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
    bullet.visual.tint = {255, 120, 60, 255};
    bullet.visual.visible = true;
    bullet.visual.UpdateOBB();

    this->bullets.push_back(bullet);
    TraceLog(LOG_INFO, "ShooterEnemy spawned bullet, active=%zu", this->bullets.size());
}

void ShooterEnemy::updateBullets(UpdateContext &uc, float deltaSeconds)
{
    auto staticObjects = uc.scene->getStaticObjects();
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
            CollisionResult hitResult = {uc.player, true, 0.0f, Vector3Zero()};
            DamageResult damage(this->bulletDamage, hitResult);
            uc.player->damage(damage);
            Vector3 knockDir = Vector3Normalize(bullet.velocity);
            if (Vector3LengthSqr(knockDir) < 0.0001f)
                knockDir = {0.0f, 0.0f, 1.0f};
            uc.player->applyKnockback(Vector3Scale(knockDir, 5.0f), 0.2f, 0.0f);
            return true;
        }

        for (auto *obj : staticObjects)
        {
            if (!obj)
                continue;
            if (CheckCollisionSphereVsOBB(bullet.position, bullet.radius, &obj->obb))
            {
                return true;
            }
        }

        return false;
    });

    this->bullets.erase(removeIt, this->bullets.end());
}

void ShooterEnemy::gatherObjects(std::vector<Object *> &out) const
{
    Enemy::gatherObjects(out);
    for (const auto &bullet : this->bullets)
    {
        out.push_back(const_cast<Object *>(&bullet.visual));
    }
}
