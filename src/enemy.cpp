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

    if (!this->isKnockbackActive())
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
        
        settings.facingHint = toPlayer;
    }

    this->UpdateCommonBehavior(uc, desiredDir, delta, settings);
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
    
    for (int i = 0; i < count; ++i)
    {
        float angle = (2.0f * PI) * ((float)i / (float)count);
        Vector3 offset = {cosf(angle) * radius, 0.0f, sinf(angle) * radius};
        Vector3 spawnPos = Vector3Add(this->position, offset);
        MinionEnemy *m = new MinionEnemy();
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
        break;
    }

    case SummonState::Summoning:
    {
        animationTimer += delta;
        
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
    
    // Always update animation (handles idle countdown and active animation states)
    UpdateSummonAnimation(uc, delta);
    
    // Handle summoning animation if active
    if (summonState != SummonState::Idle)
    {
        // During animation, only do basic dialog
        this->UpdateDialog(uc);
        return;
    }
    
    // Normal idle behavior
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

    // Retreat if player too close
    if (dist < retreatDistance)
    {
        desiredDir = Vector3Normalize(Vector3Negate(toPlayer));
        settings.facingHint = Vector3Negate(desiredDir);
    }
    else
    {
        desiredDir = Vector3Zero();
        settings.facingHint = toPlayer;
    }

    this->UpdateCommonBehavior(uc, desiredDir, delta, settings);
    this->UpdateDialog(uc);
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

void SupportEnemy::FindHealTarget(UpdateContext &uc)
{
    // Find the lowest HP ally that's below healing threshold
    std::vector<Entity *> enemies = uc.scene->em.getEntities(ENTITY_ENEMY);
    Enemy* bestTarget = nullptr;
    float lowestHealthPercent = 1.0f;
    
    for (Entity *entity : enemies)
    {
        if (!entity || entity == this) continue;
        
        Enemy *ally = dynamic_cast<Enemy*>(entity);
        if (!ally) continue;
        
        // Skip minions
        if (dynamic_cast<MinionEnemy*>(ally)) continue;
        
        // Check if in range
        float dist = Vector3Distance(this->position, ally->pos());
        if (dist > healingRange) continue;
        
        // Check if below healing threshold
        float healthPercent = (float)ally->getHealth() / (float)ally->getMaxHealth();
        if (healthPercent >= healingThreshold) continue;
        
        // Track lowest health ally
        if (healthPercent < lowestHealthPercent)
        {
            lowestHealthPercent = healthPercent;
            bestTarget = ally;
        }
    }
    
    this->targetAlly = bestTarget;
}

void SupportEnemy::FindHideTarget(UpdateContext &uc)
{
    // Find a strong enemy to hide behind (not a minion)
    std::vector<Entity *> enemies = uc.scene->em.getEntities(ENTITY_ENEMY);
    Enemy* bestHideTarget = nullptr;
    float closestDist = 999999.0f;
    
    for (Entity *entity : enemies)
    {
        if (!entity || entity == this) continue;
        
        Enemy *ally = dynamic_cast<Enemy*>(entity);
        if (!ally) continue;
        
        // Skip minions - we want to hide behind stronger enemies
        if (dynamic_cast<MinionEnemy*>(ally)) continue;
        
        // Prefer tanks
        ChargingEnemy* tank = dynamic_cast<ChargingEnemy*>(ally);
        float dist = Vector3Distance(this->position, ally->pos());
        
        if (tank && dist < 20.0f)
        {
            bestHideTarget = ally;
            closestDist = dist;
            break; // Tanks are priority
        }
        
        // Otherwise, find closest non-minion ally
        if (dist < closestDist && dist < 25.0f)
        {
            closestDist = dist;
            bestHideTarget = ally;
        }
    }
    
    this->hideTarget = bestHideTarget;
}

void SupportEnemy::ApplyHealing(UpdateContext &uc, Enemy* target, float delta)
{
    if (!target) return;
    
    // Charge up heal glow
    this->healGlowTimer += delta * 2.0f; // Takes 0.5 seconds to charge
    
    if (this->healGlowTimer >= 1.0f)
    {
        // Apply healing
        float healAmount = healingRate * delta;
        target->Heal((int)healAmount);
        this->isHealing = true;
        
        // Spawn yellow/gold healing particles
        if (uc.scene)
        {
            // Particles flow from support to target
            Vector3 direction = Vector3Subtract(target->pos(), this->position);
            Vector3 midpoint = Vector3Add(this->position, Vector3Scale(direction, 0.5f));
            uc.scene->particles.spawnDirectional(this->position, direction, 5, GOLD, 3.0f, 0.2f);
            uc.scene->particles.spawnExplosion(target->pos(), 3, YELLOW, 0.2f, 1.0f, 0.3f);
        }
        
        // Reset if health is full or out of range
        float healthPercent = (float)target->getHealth() / (float)target->getMaxHealth();
        if (healthPercent >= healingThreshold)
        {
            this->healGlowTimer = 0.0f;
            this->isHealing = false;
        }
    }
    else
    {
        this->isHealing = false;
    }
}

void SupportEnemy::ApplySpeedBuffs(UpdateContext &uc)
{
    // Apply speed buff to all nearby allies
    std::vector<Entity *> enemies = uc.scene->em.getEntities(ENTITY_ENEMY);
    
    int buffedCount = 0;
    for (Entity *entity : enemies)
    {
        if (!entity || entity == this) continue;
        
        Enemy *ally = dynamic_cast<Enemy*>(entity);
        if (!ally) continue;
        
        // Check if in range
        float dist = Vector3Distance(this->position, ally->pos());
        if (dist > speedBuffRange) continue;
        
        // Apply speed buff (would need to modify Enemy class to have speedMultiplier)
        // Spawn blue buff particles around buffed allies
        if (uc.scene && buffedCount < 3) // Limit particle spam
        {
            uc.scene->particles.spawnExplosion(ally->pos(), 2, SKYBLUE, 0.15f, 1.5f, 0.5f);
            buffedCount++;
        }
    }
}

void SupportEnemy::UpdatePositioning(UpdateContext &uc, const Vector3 &toPlayer)
{
    // Try to stay behind Tank allies or other strong enemies
    // For now, just retreat if player too close
}

void SupportEnemy::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    
    Vector3 toPlayer = Vector3Subtract(uc.player->pos(), this->position);
    toPlayer.y = 0.0f;
    float dist = Vector3Length(toPlayer);
    Vector3 desiredDir = Vector3Zero();

    Enemy::MovementSettings settings;
    settings.lockToGround = true;
    settings.maxSpeed = 3.0f;  // Slower than other enemies
    settings.maxAccel = MAX_ACCEL;
    settings.decelGround = FRICTION;
    settings.decelAir = AIR_DRAG;

    // First, look for allies to heal
    FindHealTarget(uc);
    
    // If we have a heal target, move toward it
    if (this->targetAlly)
    {
        Vector3 toAlly = Vector3Subtract(this->targetAlly->pos(), this->position);
        toAlly.y = 0.0f;
        float allyDist = Vector3Length(toAlly);
        
        if (allyDist > 0.1f && allyDist > 8.0f)  // Move closer if too far
        {
            desiredDir = Vector3Normalize(toAlly);
            settings.facingHint = desiredDir;
        }
        
        // Apply healing
        ApplyHealing(uc, this->targetAlly, delta);
    }
    else
    {
        // Reset heal glow when not healing
        this->healGlowTimer = 0.0f;
        this->isHealing = false;
        
        // Find an ally to hide behind
        FindHideTarget(uc);
        
        if (this->hideTarget)
        {
            // Position ourselves so the ally is between us and the player
            Vector3 allyPos = this->hideTarget->pos();
            Vector3 playerToAlly = Vector3Subtract(allyPos, uc.player->pos());
            playerToAlly.y = 0.0f;
            
            if (Vector3Length(playerToAlly) > 0.1f)
            {
                Vector3 hideSpot = Vector3Add(allyPos, Vector3Scale(Vector3Normalize(playerToAlly), 5.0f));
                Vector3 toHideSpot = Vector3Subtract(hideSpot, this->position);
                toHideSpot.y = 0.0f;
                float hideSpotDist = Vector3Length(toHideSpot);
                
                if (hideSpotDist > 1.0f)
                {
                    desiredDir = Vector3Normalize(toHideSpot);
                    settings.facingHint = toPlayer; // Face toward player while hiding
                }
            }
        }
        else if (dist < retreatDistance)
        {
            // No allies to hide behind, just retreat
            desiredDir = Vector3Normalize(Vector3Negate(toPlayer));
            settings.facingHint = Vector3Negate(desiredDir);
        }
        else
        {
            // Idle, look at player
            desiredDir = Vector3Zero();
            settings.facingHint = toPlayer;
        }
    }
    
    // Apply speed buffs to nearby allies
    ApplySpeedBuffs(uc);
    this->buffGlowTimer += delta;

    this->UpdateCommonBehavior(uc, desiredDir, delta, settings);
    this->UpdateDialog(uc);
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
    // Particles are now handled by the particle system in UpdateBody
}
