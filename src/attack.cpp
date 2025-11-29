#include "attack.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <raymath.h>
#include "updateContext.hpp"
#include <algorithm>
#include "scene.hpp"

// Helper: create quaternion that faces forward (used by setRotationFromForward / quaternion helpers already in file)
Quaternion GetQuaternionFromForward(Vector3 forward)
{
    Vector3 modelForward = {0.0f, 0.0f, 1.0f};
    Vector3 fNorm = Vector3Normalize(forward);
    float angleRad = Vector3Angle(modelForward, fNorm);
    Vector3 rotAxis = Vector3CrossProduct(modelForward, fNorm);
    if (Vector3LengthSqr(rotAxis) < 1e-6f) // Use Sqr for efficiency
    {
        rotAxis = {0.0f, 1.0f, 0.0f};
    }
    rotAxis = Vector3Normalize(rotAxis);
    return QuaternionFromAxisAngle(rotAxis, angleRad);
}

// --- ThousandTileAttack: thousand-mode implementation (moved from ThousandAttack) ---
void ThousandTileAttack::startThousandMode()
{
    if (this->projectiles.empty())
        return;
    this->mode = MODE_THOUSAND_FINAL;
    // compute average destination
    float x = 0, y = 0, z = 0;
    for (const auto &p : this->projectiles)
    {
        x += p.pos().x;
        y += p.pos().y;
        z += p.pos().z;
    }
    float size = (float)this->projectiles.size();
    this->thousandDest = {x / size, y / size, z / size};

    // set each projectile to fly towards dest
    for (auto &p : this->projectiles)
    {
        Vector3 toDest = Vector3Subtract(this->thousandDest, p.pos());
        Vector3 newDir = Vector3Normalize(toDest);
        Vector3 newVel = Vector3Scale(newDir, ThousandTileAttack::thousandFinalVel);
        // preserve grounded state and object rotation
        p = Projectile(p.pos(), newVel, newDir, p.isGrounded(), p.obj(), 1.0f, 1.0f, p.type);
    }
}

bool ThousandTileAttack::thousandEndFinal()
{
    // remove projectiles that reached or overshot destination, like previous endFinal
    auto should_remove = [&](const Projectile &p)
    {
        if (Vector3DistanceSqr(p.pos(), this->thousandDest) <= 0.04f) // small margin (0.2^2)
            return true;
        Vector3 toDest = Vector3Subtract(this->thousandDest, p.pos());
        if (Vector3DotProduct(toDest, p.vel()) <= 0)
            return true;
        return false;
    };

    size_t old_size = this->projectiles.size();
    this->projectiles.erase(
        std::remove_if(this->projectiles.begin(), this->projectiles.end(), should_remove),
        this->projectiles.end());
    this->count -= (old_size - this->projectiles.size());

    return this->projectiles.empty();
}

// --- Triplet integration (connectors) moved into ThousandTileAttack ---
void ThousandTileAttack::startTripletMode()
{
    if (this->projectiles.size() < 3)
        return;
    this->mode = MODE_TRIPLET_CONNECTING;
    this->connectingTimer = 0.0f;
    this->connectors.clear();
    this->connectorForward.clear();
}

// helper used inside update to perform triplet connector spawn / animation
void ThousandTileAttack::updateTriplet(UpdateContext &uc)
{
    // update physics of projectiles still
    for (auto &p : this->projectiles)
        p.UpdateBody(uc);

    connectingTimer += GetFrameTime();

    auto spawnConnector = [&](int startIdx, int endIdx)
    {
        Vector3 start = projectiles[startIdx].pos();
        Vector3 end = projectiles[endIdx].pos();
        Vector3 delta = Vector3Subtract(end, start);
        float length = Vector3Length(delta);
        Vector3 mid = Vector3Scale(Vector3Add(start, end), 0.5f);
        Vector3 forward = Vector3Normalize(delta);
        Object conn({connectorThickness, connectorThickness, length}, mid);
        conn.setRotationFromForward(forward);
        this->connectors.push_back(conn);
        this->connectorForward.push_back(0.0f);
    };

    // spawn connectors sequentially
    if (this->connectors.size() < 1 && connectingTimer > 0.0f)
    {
        spawnConnector(0, 1);
    }
    if (this->connectors.size() < 2 && connectingTimer > 0.5f)
    {
        spawnConnector(1, 2);
    }
    if (this->connectors.size() < 3 && connectingTimer > 1.0f)
    {
        spawnConnector(2, 0);
    }

    // after spawning connectors, grow them (FINAL stage)
    if (connectingTimer > 2.0f)
    {
        // transition to FINAL stage where we animate connectors growing/rotating
        this->mode = MODE_TRIPLET_FINAL;
    }
}

void ThousandTileAttack::update(UpdateContext &uc)
{
    // Always update base projectile physics if not handled by mode-specific code above
    if (this->mode != MODE_TRIPLET_CONNECTING && this->mode != MODE_TRIPLET_FINAL)
    {
        for (auto &p : this->projectiles)
            p.UpdateBody(uc);
    }

    // Mode handling
    switch (this->mode)
    {
    case MODE_THOUSAND_FINAL:
        if (thousandEndFinal())
        {
            this->mode = MODE_NORMAL;
            this->thousandActivated = false;
            this->count = 0;
            this->thousandDest = {0};
        }
        break;
    case MODE_TRIPLET_CONNECTING:
    case MODE_TRIPLET_FINAL:
    {
        // animate connectors expanding / rotating to final height
        bool allAtTarget = true;
        for (size_t i = 0; i < connectors.size(); ++i)
        {
            Object &c = connectors[i];
            if (c.size.y < tripletFinalHeight)
            {
                c.size.y += tripletGrowthRate;
                if (c.size.y >= tripletFinalHeight)
                    c.size.y = tripletFinalHeight;
                allAtTarget = false;
            }
            // rotate connector forward (visual) using rotate helper
            Vector3 forwardDir = Vector3RotateByQuaternion({0.0f, 0.0f, 1.0f}, c.getRotation());
            c.rotate(forwardDir, 1.0f); // small rotation per frame
        }
        if (allAtTarget)
        {
            // cleanup and reset
            this->projectiles.clear();
            this->connectors.clear();
            this->connectorForward.clear();
            this->mode = MODE_NORMAL;
            this->count = 0;
        }
    }
    break;
    default:
        // MODE_NORMAL etc. nothing special
        break;
    }
}
void ThousandTileAttack::spawnProjectile(UpdateContext &uc)
{
    if (!uc.scene)
        return;
    AttackManager &am = uc.scene->am;
    if (am.isAttackLockedByOther(this))
        return;

    float yaw;
    float pitch;

    // Determine spawner orientation without RTTI: use category() then static_cast
    if (this->spawnedBy && this->spawnedBy->category() == ENTITY_PLAYER)
    {
        Me *player = static_cast<Me *>(this->spawnedBy);
        yaw = player->getLookRotation().x;
        pitch = player->getLookRotation().y;
    }
    else if (this->spawnedBy && this->spawnedBy->category() == ENTITY_ENEMY)
    {
        Enemy *enemy = static_cast<Enemy *>(this->spawnedBy);
        yaw = enemy->dir().x;
        pitch = enemy->dir().y;
    }
    else
    {
        return;
    }

    Vector3 forward = {
        cosf(pitch) * sinf(yaw),
        0.0f,
        cosf(pitch) * cosf(yaw)};
    forward = Vector3Normalize(forward);

    Vector3 vel = {-forward.x * shootHoriSpeed + this->spawnedBy->vel().x, shootVertSpeed, -forward.z * shootHoriSpeed + this->spawnedBy->vel().z};
    Vector3 pos{this->spawnedBy->pos().x, this->spawnedBy->pos().y + 1.8f, this->spawnedBy->pos().z};
    Object o({1, 1, 1}, pos);
    o.setRotationFromForward(forward);
    o.useTexture = true;

    const auto tile = uc.uiManager->muim.getSelectedTile();
    o.texture = &uc.uiManager->muim.getSpriteSheet();
    o.sourceRect = uc.uiManager->muim.getTile(tile);

    Projectile projectile(
        pos,
        vel,
        forward,
        false,
        o,
        FRICTION,
        AIR_DRAG,
        tile);

    this->projectiles.push_back(projectile);
    uc.scene->am.recordThrow(uc);
    // this->lifetime.push_back(2.0f); // 2 seconds lifetime
}

// --- MeleePushAttack ------------------------------------------------------------------
void MeleePushAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();

    if (this->cooldownRemaining > 0.0f)
    {
        this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);
    }

    if (this->pendingStrike)
    {
        this->windupRemaining = fmaxf(0.0f, this->windupRemaining - delta);
        if (this->windupRemaining <= 0.0f)
        {
            this->performStrike(uc);
        }
    }

    this->updateTileIndicator(uc, delta);

    for (auto &volume : this->effectVolumes)
    {
        volume.remainingLife -= delta;
    }

    this->effectVolumes.erase(
        std::remove_if(
            this->effectVolumes.begin(),
            this->effectVolumes.end(),
            [](const EffectVolume &v)
            {
                return v.remainingLife <= 0.0f;
            }),
        this->effectVolumes.end());
}

std::vector<Object *> MeleePushAttack::obj() const
{
    std::vector<Object *> ret;
    ret.reserve(this->effectVolumes.size() + (this->tileIndicator.active ? 1U : 0U));
    for (const auto &volume : this->effectVolumes)
    {
        ret.push_back(const_cast<Object *>(&volume.area));
    }
    if (this->tileIndicator.active)
    {
        ret.push_back(const_cast<Object *>(&this->tileIndicator.sprite));
    }
    return ret;
}

Vector3 MeleePushAttack::getForwardVector() const
{
    if (this->spawnedBy)
    {
        if (this->spawnedBy->category() == ENTITY_PLAYER)
        {
            const Me *player = static_cast<const Me *>(this->spawnedBy);
            float yaw = player->getLookRotation().x;
            Vector3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
            return Vector3Normalize(forward);
        }
        if (this->spawnedBy->category() == ENTITY_ENEMY)
        {
            const Enemy *enemy = static_cast<const Enemy *>(this->spawnedBy);
            Vector3 dir = enemy->dir();
            Vector3 forward = {dir.x, 0.0f, dir.z};
            if (Vector3LengthSqr(forward) < 0.0001f)
                forward = {0.0f, 0.0f, 1.0f};
            return Vector3Normalize(forward);
        }
    }
    return {0.0f, 0.0f, 1.0f};
}

Vector3 MeleePushAttack::getPlayerCameraForward() const
{
    if (this->spawnedBy && this->spawnedBy->category() == ENTITY_PLAYER)
    {
        const Me *player = static_cast<const Me *>(this->spawnedBy);
        const Camera &camera = player->getCamera();
        Vector3 forward = Vector3Subtract(camera.target, camera.position);
        if (Vector3LengthSqr(forward) >= 0.0001f)
        {
            return Vector3Normalize(forward);
        }
    }
    return this->getForwardVector();
}

Vector3 MeleePushAttack::getPlayerCameraPosition() const
{
    if (this->spawnedBy)
    {
        if (this->spawnedBy->category() == ENTITY_PLAYER)
        {
            const Me *player = static_cast<const Me *>(this->spawnedBy);
            return player->getCamera().position;
        }
        Vector3 pos = this->spawnedBy->pos();
        pos.y += indicatorYOffset;
        return pos;
    }
    return {0.0f, indicatorYOffset, 0.0f};
}

MeleePushAttack::EffectVolume MeleePushAttack::buildEffectVolume(const Vector3 &origin, const Vector3 &forward) const
{
    /*
     * Build a short-lived effect volume representing the melee sweep.
     * The volume is an oriented box (OBB) placed half-way along the
     * `pushRange` in front of the attacker, with width computed from
     * the swing angle so it approximates a fan / cone sweep.
     *
     * This Object is then used both for collision testing (OBB vs OBB)
     * and for visualization (rendered as a temporary cube by Scene).
     */
    EffectVolume volume;
    Vector3 forwardNorm = forward;
    if (Vector3LengthSqr(forwardNorm) < 0.0001f)
    {
        forwardNorm = {0.0f, 0.0f, 1.0f};
    }
    else
    {
        forwardNorm = Vector3Normalize(forwardNorm);
    }

    float halfAngle = pushAngle * 0.5f;
    float sweepWidth = 2.0f * pushRange * tanf(halfAngle);
    Vector3 size = {sweepWidth, effectHeight, pushRange};
    Vector3 center = Vector3Add(origin, Vector3Scale(forwardNorm, -pushRange * 0.5f));
    center.y = origin.y + effectYOffset;

    // Create the visual/physical Object representing the area and cache its OBB
    volume.area = Object(size, center);
    volume.area.setVisible(false);
    // Orient the box so its local +Z axis points along the forward direction
    volume.area.setRotationFromForward(forwardNorm);
    // Update the OBB used by collision helpers
    volume.area.UpdateOBB();
    // Short lifetime so the visual feedback disappears quickly
    volume.remainingLife = effectLifetime;

    return volume;
}

bool MeleePushAttack::pushEnemies(UpdateContext &uc, EffectVolume &volume)
{
    if (!uc.scene)
        return false;

    bool hit = false;
    for (auto entity : uc.scene->getEntities(ENTITY_ENEMY))
    {
        if (!entity || entity == this->spawnedBy)
            continue;
        Enemy *enemy = static_cast<Enemy *>(entity);
        CollisionResult collision = Object::collided(volume.area, enemy->obj());
        if (!collision.collided)
            continue;

        Vector3 origin = this->spawnedBy->pos();
        Vector3 delta = Vector3Subtract(enemy->pos(), origin);
        delta.y = 0.0f;
        float distSq = Vector3LengthSqr(delta);

        Vector3 dirNorm;
        if (distSq > 0.0001f)
        {
            dirNorm = Vector3Scale(delta, 1.0f / sqrtf(distSq));
        }
        else
        {
            Quaternion q = volume.area.getRotation();
            dirNorm = Vector3RotateByQuaternion({0.0f, 0.0f, 1.0f}, q);
        }

        Vector3 push = Vector3Scale(dirNorm, pushForce);
        enemy->applyKnockback(push, knockbackDuration, verticalLift);
        hit = true;
    }
    return hit;
}

void MeleePushAttack::trigger(UpdateContext &uc)
{
    if (this->cooldownRemaining > 0.0f || this->pendingStrike)
        return;

    if (!uc.scene)
        return;
    AttackManager &am = uc.scene->am;
    if (!am.tryLockAttack(this))
        return;

    this->pendingStrike = true;
    this->windupRemaining = windupDuration;
    this->requestPlayerWindupLock();
    this->initializeTileIndicator(uc);
}

void MeleePushAttack::performStrike(UpdateContext &uc)
{
    Vector3 origin = this->spawnedBy->pos();
    Vector3 forward = this->getForwardVector();
    Vector3 cameraForward = this->getPlayerCameraForward();
    EffectVolume volume = this->buildEffectVolume(origin, forward);
    bool hit = this->pushEnemies(uc, volume);
    this->effectVolumes.push_back(volume);
    this->launchTileIndicator(this->getPlayerCameraPosition(), cameraForward);

    this->pendingStrike = false;
    this->cooldownRemaining = cooldownDuration;
    this->providePlayerFeedback(hit);
    if (uc.scene)
    {
        uc.scene->am.releaseAttackLock(this);
    }
}

void MeleePushAttack::requestPlayerWindupLock()
{
    if (!this->spawnedBy || this->spawnedBy->category() != ENTITY_PLAYER)
        return;
    Me *player = static_cast<Me *>(this->spawnedBy);
    player->beginMeleeWindup(windupDuration);
}

void MeleePushAttack::providePlayerFeedback(bool hit)
{
    if (!this->spawnedBy)
        return;
    if (this->spawnedBy->category() == ENTITY_PLAYER)
    {
        Me *player = static_cast<Me *>(this->spawnedBy);
        player->triggerMeleeSwing(swingDuration);
        float shakeMag = cameraShakeMagnitude * (hit ? 1.0f : 0.5f);
        player->addCameraShake(shakeMag, cameraShakeDuration);
    }
}

void MeleePushAttack::initializeTileIndicator(UpdateContext &uc)
{
    if (!this->spawnedBy || this->spawnedBy->category() != ENTITY_PLAYER)
        return;
    if (!uc.uiManager)
        return;

    Texture2D &sheet = uc.uiManager->muim.getSpriteSheet();
    MahjongTileType tile = uc.uiManager->muim.getSelectedTile();

    this->tileIndicator.sprite.size = {indicatorWidth, indicatorHeight, indicatorThickness};
    this->tileIndicator.sprite.useTexture = true;
    this->tileIndicator.sprite.texture = &sheet;
    this->tileIndicator.sprite.sourceRect = uc.uiManager->muim.getTile(tile);
    this->tileIndicator.sprite.setVisible(true);

    this->tileIndicator.active = true;
    this->tileIndicator.launched = false;
    this->tileIndicator.travelProgress = 0.0f;
    this->tileIndicator.opacity = indicatorStartOpacity;

    Vector3 forward = this->getPlayerCameraForward();
    this->tileIndicator.forward = forward;
    Vector3 cameraPos = this->getPlayerCameraPosition();
    Vector3 holdPos = Vector3Add(cameraPos, Vector3Scale(forward, indicatorHoldDistance));
    this->tileIndicator.startPos = holdPos;
    this->tileIndicator.targetPos = holdPos;
    this->tileIndicator.sprite.pos = holdPos;
    Vector3 faceDir = Vector3Scale(forward, -1.0f);
    this->tileIndicator.sprite.setRotationFromForward(faceDir);

    float alphaF = Clamp(indicatorStartOpacity * 255.0f, 0.0f, 255.0f);
    this->tileIndicator.sprite.tint = {255, 255, 255, (unsigned char)alphaF};
    this->tileIndicator.sprite.UpdateOBB();
}

void MeleePushAttack::updateTileIndicator(UpdateContext &uc, float deltaSeconds)
{
    (void)uc;
    if (!this->tileIndicator.active)
        return;
    if (!this->spawnedBy)
    {
        this->deactivateTileIndicator();
        return;
    }

    Vector3 cameraPos = this->getPlayerCameraPosition();
    Vector3 cameraForward = this->getPlayerCameraForward();

    if (!this->tileIndicator.launched)
    {
        this->tileIndicator.forward = cameraForward;
        Vector3 holdPos = Vector3Add(cameraPos, Vector3Scale(cameraForward, indicatorHoldDistance));
        this->tileIndicator.startPos = holdPos;
        this->tileIndicator.sprite.pos = holdPos;
        Vector3 faceDir = Vector3Scale(cameraForward, -1.0f);
        this->tileIndicator.sprite.setRotationFromForward(faceDir);
        this->tileIndicator.sprite.UpdateOBB();

        float progress = (windupDuration > 0.0f) ? 1.0f - (this->windupRemaining / windupDuration) : 1.0f;
        progress = Clamp(progress, 0.0f, 1.0f);
        float opacity = Lerp(indicatorStartOpacity, 1.0f, progress);
        this->tileIndicator.opacity = opacity;
        this->tileIndicator.sprite.tint.a = (unsigned char)Clamp(opacity * 255.0f, 0.0f, 255.0f);

        if (!this->pendingStrike)
        {
            this->deactivateTileIndicator();
        }
    }
    else
    {
        if (indicatorTravelDuration <= 0.0f)
        {
            this->tileIndicator.sprite.pos = this->tileIndicator.targetPos;
            this->tileIndicator.sprite.UpdateOBB();
            this->deactivateTileIndicator();
            return;
        }

        this->tileIndicator.travelProgress += deltaSeconds / indicatorTravelDuration;
        float t = Clamp(this->tileIndicator.travelProgress, 0.0f, 1.0f);
        Vector3 pos = Vector3Lerp(this->tileIndicator.startPos, this->tileIndicator.targetPos, t);
        this->tileIndicator.sprite.pos = pos;
        Vector3 faceDir = Vector3Scale(cameraForward, -1.0f);
        this->tileIndicator.sprite.setRotationFromForward(faceDir);
        this->tileIndicator.sprite.UpdateOBB();
        this->tileIndicator.sprite.tint.a = 255;

        if (this->tileIndicator.travelProgress >= 1.0f)
        {
            this->deactivateTileIndicator();
        }
    }
}

void MeleePushAttack::launchTileIndicator(const Vector3 &origin, const Vector3 &forward)
{
    if (!this->tileIndicator.active)
        return;

    this->tileIndicator.launched = true;
    this->tileIndicator.travelProgress = 0.0f;
    this->tileIndicator.forward = forward;
    this->tileIndicator.startPos = this->tileIndicator.sprite.pos;
    Vector3 target = Vector3Add(origin, Vector3Scale(forward, pushRange));
    if (!this->spawnedBy || this->spawnedBy->category() != ENTITY_PLAYER)
    {
        target.y = origin.y + indicatorYOffset;
    }
    this->tileIndicator.targetPos = target;
    this->tileIndicator.sprite.tint.a = 255;
    this->tileIndicator.sprite.setVisible(true);
    Vector3 faceDir = Vector3Scale(this->getPlayerCameraForward(), -1.0f);
    this->tileIndicator.sprite.setRotationFromForward(faceDir);
    this->tileIndicator.sprite.UpdateOBB();
}

void MeleePushAttack::deactivateTileIndicator()
{
    this->tileIndicator.active = false;
    this->tileIndicator.launched = false;
    this->tileIndicator.sprite.setVisible(false);
}