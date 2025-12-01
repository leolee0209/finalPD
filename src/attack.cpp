#include "attack.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <raymath.h>
#include "updateContext.hpp"
#include <algorithm>
#include "scene.hpp"

Texture2D DotBombAttack::explosionTexture{};
bool DotBombAttack::explosionTextureLoaded = false;
int DotBombAttack::explosionTextureUsers = 0;

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

// --- DotBombAttack --------------------------------------------------------------------

DotBombAttack::DotBombAttack(Entity *_spawnedBy) : AttackController(_spawnedBy)
{
    retainExplosionTexture();
}

DotBombAttack::~DotBombAttack()
{
    releaseExplosionTexture();
}

bool DotBombAttack::trigger(UpdateContext &uc, MahjongTileType tile)
{
    if (!uc.scene || !this->spawnedBy)
        return false;

    if (this->cooldownRemaining > 0.0f)
        return false;

    Vector3 forward = {0.0f, 0.0f, -1.0f};
    if (this->spawnedBy->category() == ENTITY_PLAYER)
    {
        Me *player = static_cast<Me *>(this->spawnedBy);
        const Camera &camera = player->getCamera();
        forward = Vector3Subtract(camera.target, camera.position);
    }
    else if (this->spawnedBy->category() == ENTITY_ENEMY)
    {
        Enemy *enemy = static_cast<Enemy *>(this->spawnedBy);
        forward = enemy->dir();
    }

    if (Vector3LengthSqr(forward) < 0.0001f)
    {
        forward = {0.0f, 0.0f, -1.0f};
    }
    forward = Vector3Normalize(forward);

    Vector3 pos = this->spawnedBy->pos();
    pos.y += muzzleHeight;
    Vector3 planarForward = {forward.x, 0.0f, forward.z};
    if (Vector3LengthSqr(planarForward) > 0.0001f)
    {
        planarForward = Vector3Normalize(planarForward);
        pos = Vector3Add(pos, Vector3Scale(planarForward, muzzleForwardOffset));
    }

    Vector3 velocity = Vector3Scale(forward, projectileSpeed);
    velocity.y += projectileLift;

    Object body({projectileRadius * 2.0f, projectileHeight, projectileLength}, pos);
    body.setRotationFromForward(forward);
    body.useTexture = uc.uiManager != nullptr;
    if (uc.uiManager)
    {
        body.texture = &uc.uiManager->muim.getSpriteSheet();
        body.sourceRect = uc.uiManager->muim.getTile(tile);
        body.tint = WHITE;
    }
    body.UpdateOBB();

    Bomb bomb;
    bomb.projectile = Projectile(
        pos,
        velocity,
        forward,
        false,
        body,
        projectileFriction,
        projectileDrag,
        tile);
    bomb.flightTimeRemaining = 4.0f;
    bombs.push_back(bomb);
    this->cooldownRemaining = cooldownDuration;
    return true;
}

void DotBombAttack::retainExplosionTexture()
{
    if (++explosionTextureUsers > 1)
        return;
    explosionTexture = LoadTexture(explosionTexturePath);
    explosionTextureLoaded = explosionTexture.id != 0;
    if (!explosionTextureLoaded)
    {
        TraceLog(LOG_WARNING, "Failed to load explosion texture from %s", explosionTexturePath);
    }
}

void DotBombAttack::releaseExplosionTexture()
{
    if (explosionTextureUsers <= 0)
        return;
    explosionTextureUsers--;
    if (explosionTextureUsers == 0)
    {
        if (explosionTextureLoaded && IsWindowReady())
        {
            UnloadTexture(explosionTexture);
        }
        explosionTexture = Texture2D{};
        explosionTextureLoaded = false;
    }
}

void DotBombAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);
    for (auto &bomb : bombs)
    {
        if (!bomb.exploded)
        {
            bomb.flightTimeRemaining = fmaxf(0.0f, bomb.flightTimeRemaining - delta);
            bomb.projectile.UpdateBody(uc);

            bool hitEnvironment = false;
            if (uc.scene)
            {
                const auto worldHits = Object::collided(bomb.projectile.obj(), uc.scene);
                hitEnvironment = std::any_of(
                    worldHits.begin(),
                    worldHits.end(),
                    [](const CollisionResult &hit)
                    {
                        return hit.with == nullptr; // static world geometry
                    });
            }

            bool hitEnemy = false;
            if (uc.scene && !hitEnvironment)
            {
                for (Entity *entity : uc.scene->em.getEntities())
                {
                    if (!entity || entity->category() != ENTITY_ENEMY)
                        continue;
                    Enemy *enemy = static_cast<Enemy *>(entity);
                    CollisionResult result = Object::collided(bomb.projectile.obj(), enemy->obj());
                    if (result.collided)
                    {
                        hitEnemy = true;
                        break;
                    }
                }
            }

            bool timedOut = bomb.flightTimeRemaining <= 0.0f;
            bool groundedHit = bomb.projectile.pos().y <= 0.2f && bomb.projectile.vel().y <= 0.0f;

            if (hitEnvironment || hitEnemy || groundedHit || timedOut)
            {
                startExplosion(bomb, bomb.projectile.pos(), uc);
            }
        }
        else
        {
            bomb.explosionTimer -= delta;
            float normalized = 1.0f - (bomb.explosionTimer / explosionLifetime);
            normalized = Clamp(normalized, 0.0f, 1.0f);
            float radius = explosionStartRadius + (explosionEndRadius - explosionStartRadius) * normalized;
            bomb.explosionFx.pos = {bomb.explosionOrigin.x, fmaxf(bomb.explosionOrigin.y, 0.0f), bomb.explosionOrigin.z};
            bomb.explosionFx.setAsSphere(radius);

            unsigned char alpha = (unsigned char)Clamp((1.0f - normalized) * 220.0f, 0.0f, 255.0f);
            bomb.explosionFx.tint = {255, 190, 90, alpha};
            updateExplosionBillboard(bomb, uc, normalized);
        }
    }

    bombs.erase(
        std::remove_if(
            bombs.begin(),
            bombs.end(),
            [](const Bomb &bomb)
            {
                return bomb.exploded && bomb.explosionTimer <= 0.0f;
            }),
        bombs.end());
}

float DotBombAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 0.0f;
    return Clamp(this->cooldownRemaining / cooldownDuration, 0.0f, 1.0f);
}

std::vector<Entity *> DotBombAttack::getEntities()
{
    std::vector<Entity *> ret;
    for (auto &bomb : bombs)
    {
        if (!bomb.exploded)
        {
            ret.push_back(&bomb.projectile);
        }
    }
    return ret;
}

std::vector<Object *> DotBombAttack::obj()
{
    std::vector<Object *> ret;
    for (auto &bomb : bombs)
    {
        if (!bomb.exploded)
        {
            ret.push_back(&bomb.projectile.obj());
        }
        if (bomb.fxActive)
        {
            ret.push_back(&bomb.explosionFx);
        }
        if (bomb.spriteActive)
        {
            ret.push_back(&bomb.explosionSprite);
        }
    }
    return ret;
}

void DotBombAttack::startExplosion(Bomb &bomb, const Vector3 &origin, UpdateContext &uc)
{
    if (bomb.exploded)
        return;
    bomb.exploded = true;
    bomb.explosionTimer = explosionLifetime;
    bomb.explosionOrigin = origin;
    bomb.projectile.obj().setVisible(false);
    bomb.fxActive = true;
    Vector3 fxPos = {origin.x, fmaxf(origin.y, 0.0f), origin.z};
    bomb.explosionFx = Object({1.0f, 1.0f, 1.0f}, fxPos);
    bomb.explosionFx.setAsSphere(explosionStartRadius);
    bomb.explosionFx.setVisible(true);
    bomb.explosionFx.useTexture = false;
    bomb.explosionFx.tint = {255, 190, 90, 220};
    bomb.explosionSprite = Object({explosionSpriteStartSize, explosionSpriteStartSize, explosionSpriteDepth}, fxPos);
    bomb.explosionSprite.setVisible(true);
    bomb.explosionSprite.useTexture = explosionTextureLoaded;
    if (explosionTextureLoaded)
    {
        bomb.explosionSprite.texture = &explosionTexture;
        bomb.explosionSprite.sourceRect = {0.0f, 0.0f, (float)explosionTexture.width, (float)explosionTexture.height};
    }
    else
    {
        bomb.explosionSprite.texture = nullptr;
    }
    bomb.explosionSprite.tint = WHITE;
    bomb.explosionSprite.UpdateOBB();
    bomb.spriteActive = true;
    applyExplosionEffects(origin, uc);
}

void DotBombAttack::applyExplosionEffects(const Vector3 &origin, UpdateContext &uc)
{
    if (!uc.scene)
        return;

    const float maxRadiusSq = explosionEndRadius * explosionEndRadius;
    auto applyToEntity = [&](Entity *entity)
    {
        if (!entity)
            return;
        EntityCategory category = entity->category();
        if (category != ENTITY_ENEMY && category != ENTITY_PLAYER)
            return;

        Vector3 delta = Vector3Subtract(entity->pos(), origin);
        float distanceSq = Vector3LengthSqr(delta);
        if (distanceSq > maxRadiusSq)
            return;

        Vector3 horizontal = {delta.x, 0.0f, delta.z};
        if (Vector3LengthSqr(horizontal) < 0.0001f)
        {
            horizontal = {0.0f, 0.0f, 1.0f};
        }
        horizontal = Vector3Normalize(horizontal);
        Vector3 push = Vector3Scale(horizontal, explosionKnockback);

        Vector3 normal = delta;
        if (Vector3LengthSqr(normal) < 0.0001f)
        {
            normal = {0.0f, 1.0f, 0.0f};
        }
        else
        {
            normal = Vector3Normalize(normal);
        }

        CollisionResult impact{};
        impact.with = entity;
        impact.collided = true;
        impact.penetration = 0.0f;
        impact.normal = normal;
        DamageResult damage(explosionDamage, impact);

        if (category == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(entity);
            enemy->applyKnockback(push, explosionKnockbackDuration, explosionLift);
            uc.scene->em.damage(enemy, damage);
        }
        else if (category == ENTITY_PLAYER)
        {
            Me *player = static_cast<Me *>(entity);
            player->applyKnockback(push, explosionKnockbackDuration, explosionLift);
            player->damage(damage);
        }
    };

    if (uc.player)
    {
        applyToEntity(uc.player);
    }

    for (Entity *entity : uc.scene->em.getEntities())
    {
        applyToEntity(entity);
    }
}

void DotBombAttack::updateExplosionBillboard(Bomb &bomb, UpdateContext &uc, float normalizedProgress)
{
    if (!bomb.spriteActive)
        return;

    float spriteSize = explosionSpriteStartSize + (explosionSpriteEndSize - explosionSpriteStartSize) * normalizedProgress;
    bomb.explosionSprite.size = {spriteSize, spriteSize, explosionSpriteDepth};
    float currentRadius = explosionStartRadius + (explosionEndRadius - explosionStartRadius) * normalizedProgress;
    float minY = fmaxf(bomb.explosionOrigin.y, 0.0f);
    Vector3 spritePos = {bomb.explosionOrigin.x, fmaxf(minY + currentRadius + spriteSize * 0.25f, spriteSize * 0.5f), bomb.explosionOrigin.z};

    Vector3 faceDir = {0.0f, 0.0f, -1.0f};
    if (uc.player)
    {
        const Camera &cam = uc.player->getCamera();
        faceDir = Vector3Subtract(cam.position, bomb.explosionSprite.pos);
        faceDir.y = 0.0f;
        if (Vector3LengthSqr(faceDir) < 0.0001f)
            faceDir = {0.0f, 0.0f, -1.0f};
    }
    faceDir = Vector3Normalize(faceDir);
    bomb.explosionSprite.setRotationFromForward(faceDir);
    spritePos = Vector3Add(spritePos, Vector3Scale(faceDir, 0.1f));
    bomb.explosionSprite.pos = spritePos;
    bomb.explosionSprite.UpdateOBB();

    unsigned char spriteAlpha = (unsigned char)Clamp((1.0f - normalizedProgress) * 255.0f, 0.0f, 255.0f);
    bomb.explosionSprite.tint.a = spriteAlpha;

    if (normalizedProgress >= 1.0f)
    {
        bomb.spriteActive = false;
        bomb.explosionSprite.setVisible(false);
    }
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

MeleePushAttack::ViewBasis MeleePushAttack::getIndicatorViewBasis() const
{
    ViewBasis basis{{0.0f, indicatorYOffset, 0.0f}, {0.0f, 0.0f, 1.0f}};
    if (!this->spawnedBy)
        return basis;

    if (this->spawnedBy->category() == ENTITY_PLAYER)
    {
        const Me *player = static_cast<const Me *>(this->spawnedBy);
        const Camera &camera = player->getCamera();
        basis.position = camera.position;

        Vector3 lookDir = Vector3Subtract(camera.target, camera.position);
        if (Vector3LengthSqr(lookDir) < 0.0001f)
            lookDir = {0.0f, 0.0f, 1.0f};
        basis.forward = Vector3Normalize(lookDir);
        return basis;
    }

    basis.position = this->spawnedBy->pos();
    basis.position.y += indicatorYOffset;
    basis.forward = this->getForwardVector();
    return basis;
}

void MeleePushAttack::setIndicatorPose(const Vector3 &position, const Vector3 &forward)
{
    Vector3 facing = forward;
    if (Vector3LengthSqr(facing) < 0.0001f)
        facing = {0.0f, 0.0f, 1.0f};
    facing = Vector3Scale(facing, -1.0f);

    this->tileIndicator.sprite.pos = position;
    this->tileIndicator.sprite.setRotationFromForward(facing);
    this->tileIndicator.sprite.UpdateOBB();
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

        DamageResult damage(pushDamage, collision);
        uc.scene->em.damage(enemy, damage);
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
    ViewBasis view = this->getIndicatorViewBasis();
    EffectVolume volume = this->buildEffectVolume(origin, forward);
    bool hit = this->pushEnemies(uc, volume);
    this->effectVolumes.push_back(volume);
    this->launchTileIndicator(view);

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

float MeleePushAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 0.0f;
    return Clamp(this->cooldownRemaining / cooldownDuration, 0.0f, 1.0f);
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

    ViewBasis view = this->getIndicatorViewBasis();
    this->tileIndicator.forward = view.forward;
    Vector3 holdPos = Vector3Add(view.position, Vector3Scale(view.forward, indicatorHoldDistance));
    this->tileIndicator.startPos = holdPos;
    this->tileIndicator.targetPos = holdPos;
    this->setIndicatorPose(holdPos, view.forward);

    float alphaF = Clamp(indicatorStartOpacity * 255.0f, 0.0f, 255.0f);
    this->tileIndicator.sprite.tint = {255, 255, 255, (unsigned char)alphaF};
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

    ViewBasis view = this->getIndicatorViewBasis();

    if (!this->tileIndicator.launched)
    {
        this->tileIndicator.forward = view.forward;
        Vector3 holdPos = Vector3Add(view.position, Vector3Scale(view.forward, indicatorHoldDistance));
        this->tileIndicator.startPos = holdPos;
        this->tileIndicator.targetPos = holdPos;
        this->setIndicatorPose(holdPos, view.forward);

        float progress = (windupDuration > 0.0f) ? 1.0f - (this->windupRemaining / windupDuration) : 1.0f;
        progress = Clamp(progress, 0.0f, 1.0f);
        float opacity = Lerp(indicatorStartOpacity, indicatorEndOpacity, progress);
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
            this->setIndicatorPose(this->tileIndicator.targetPos, view.forward);
            this->deactivateTileIndicator();
            return;
        }

        this->tileIndicator.travelProgress += deltaSeconds / indicatorTravelDuration;
        float t = Clamp(this->tileIndicator.travelProgress, 0.0f, 1.0f);
        Vector3 pos = Vector3Lerp(this->tileIndicator.startPos, this->tileIndicator.targetPos, t);
        this->setIndicatorPose(pos, view.forward);
        this->tileIndicator.sprite.tint.a = 255;

        if (this->tileIndicator.travelProgress >= 1.0f)
        {
            this->deactivateTileIndicator();
        }
    }
}

void MeleePushAttack::launchTileIndicator(const ViewBasis &view)
{
    if (!this->tileIndicator.active)
        return;

    this->tileIndicator.launched = true;
    this->tileIndicator.travelProgress = 0.0f;
    this->tileIndicator.forward = view.forward;
    this->tileIndicator.startPos = this->tileIndicator.sprite.pos;
    this->tileIndicator.targetPos = Vector3Add(view.position, Vector3Scale(view.forward, pushRange));
    this->tileIndicator.sprite.tint.a = (unsigned char)Clamp(indicatorEndOpacity * 255.0f, 0.0f, 255.0f);
    this->tileIndicator.sprite.setVisible(true);
    this->setIndicatorPose(this->tileIndicator.sprite.pos, view.forward);
}

void MeleePushAttack::deactivateTileIndicator()
{
    this->tileIndicator.active = false;
    this->tileIndicator.launched = false;
    this->tileIndicator.sprite.setVisible(false);
}

// --- DashAttack ------------------------------------------------------------------
Vector3 DashAttack::computeDashDirection(const UpdateContext &uc) const
{
    if (!this->spawnedBy || this->spawnedBy->category() != ENTITY_PLAYER)
        return Vector3Zero();

    const Me *player = static_cast<const Me *>(this->spawnedBy);
    Vector2 rawInput = {(float)uc.playerInput.side, (float)-uc.playerInput.forward};
    Vector2 inputDir;
    if (fabsf(rawInput.x) < 0.01f && fabsf(rawInput.y) < 0.01f)
    {
        inputDir = {0.0f, -1.0f};
    }
    else
    {
        inputDir = Vector2Normalize(rawInput);
    }
    float yaw = player->getLookRotation().x;
    Vector3 front = {sinf(yaw), 0.0f, cosf(yaw)};
    Vector3 right = {cosf(-yaw), 0.0f, sinf(-yaw)};
    Vector3 desired = {
        inputDir.x * right.x + inputDir.y * front.x,
        0.0f,
        inputDir.x * right.z + inputDir.y * front.z};

    if (Vector3LengthSqr(desired) < 0.0001f)
        return Vector3Zero();
    return Vector3Normalize(desired);
}

Vector3 DashAttack::computeCollisionAdjustedVelocity(Me *player, UpdateContext &uc, float desiredSpeed)
{
    Vector3 defaultVel = Vector3Scale(this->dashDirection, desiredSpeed);
    if (!player || !uc.scene || desiredSpeed <= 0.0f)
        return defaultVel;

    float delta = GetFrameTime();
    if (delta <= 0.0f)
        return defaultVel;

    Object probe = player->obj();
    Vector3 startPos = player->pos();
    probe.pos = startPos;
    probe.UpdateOBB();

    float remainingDistance = desiredSpeed * delta;
    float approxSize = fmaxf(fmaxf(probe.size.x, probe.size.y), probe.size.z);
    float stepSize = fmaxf(0.2f, approxSize * 0.25f);
    Vector3 currentDir = this->dashDirection;
    int guard = 0;

    while (remainingDistance > 0.0001f && Vector3LengthSqr(currentDir) > 1e-5f && guard < 256)
    {
        ++guard;
        float step = fminf(stepSize, remainingDistance);
        Vector3 move = Vector3Scale(currentDir, step);
        probe.pos = Vector3Add(probe.pos, move);
        probe.UpdateOBB();

        auto hits = Object::collided(probe, uc.scene);
        bool blocked = false;
        Vector3 blockNormal = {0.0f, 0.0f, 0.0f};
        for (const CollisionResult &hit : hits)
        {
            if (hit.collided && hit.with == nullptr)
            {
                blocked = true;
                blockNormal = hit.normal;
                break;
            }
        }

        if (blocked)
        {
            probe.pos = Vector3Subtract(probe.pos, move);
            probe.UpdateOBB();

            float normalDot = Vector3DotProduct(currentDir, blockNormal);
            Vector3 slideDir = Vector3Subtract(currentDir, Vector3Scale(blockNormal, normalDot));
            if (Vector3LengthSqr(slideDir) < 1e-4f)
            {
                remainingDistance = 0.0f;
                break;
            }
            currentDir = Vector3Normalize(slideDir);
            continue;
        }

        remainingDistance -= step;
    }

    Vector3 displacement = Vector3Subtract(probe.pos, startPos);
    float displacementLenSq = Vector3LengthSqr(displacement);
    if (displacementLenSq > 1e-6f)
    {
        Vector3 newDir = Vector3Scale(displacement, 1.0f / sqrtf(displacementLenSq));
        this->dashDirection = newDir;
    }
    else
    {
        this->dashDirection = {0.0f, 0.0f, 0.0f};
    }

    if (delta <= 0.0f || displacementLenSq < 1e-6f)
        return Vector3Zero();

    return Vector3Scale(displacement, 1.0f / delta);
}

void DashAttack::applyDashImpulse(Me *player, UpdateContext &uc)
{
    if (!player)
        return;
    Vector3 dashVelocity = this->computeCollisionAdjustedVelocity(player, uc, dashSpeed);
    Vector3 vel = player->vel();
    vel.x = dashVelocity.x;
    vel.z = dashVelocity.z;
    player->setVelocity(vel);
    player->setDirection(Vector3Zero());
    if (Vector3LengthSqr(dashVelocity) < 0.01f)
    {
        this->activeRemaining = 0.0f;
    }
}

void DashAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    if (this->cooldownRemaining > 0.0f)
        this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);

    if (this->activeRemaining > 0.0f)
    {
        this->activeRemaining = fmaxf(0.0f, this->activeRemaining - delta);
        if (this->spawnedBy && this->spawnedBy->category() == ENTITY_PLAYER)
        {
            Me *player = static_cast<Me *>(this->spawnedBy);
            this->applyDashImpulse(player, uc);
        }
    }
}

void DashAttack::trigger(UpdateContext &uc)
{
    if (this->cooldownRemaining > 0.0f || this->activeRemaining > 0.0f)
        return;

    if (!this->spawnedBy || this->spawnedBy->category() != ENTITY_PLAYER)
        return;

    Vector3 dir = this->computeDashDirection(uc);
    if (Vector3LengthSqr(dir) < 0.0001f)
        return;

    Me *player = static_cast<Me *>(this->spawnedBy);
    this->dashDirection = dir;
    this->activeRemaining = dashDuration;
    this->cooldownRemaining = dashCooldown;
    this->applyDashImpulse(player, uc);
    player->addCameraFovKick(dashFovKick, dashFovKickDuration);
}

float DashAttack::getCooldownPercent() const
{
    if (dashCooldown <= 0.0f)
        return 0.0f;
    return Clamp(this->cooldownRemaining / dashCooldown, 0.0f, 1.0f);
}