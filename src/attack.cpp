#include "attack.hpp"
#include <fstream>
#include <raylib.h>
#include "constant.hpp"
#include <raymath.h>
#include "updateContext.hpp"
#include <algorithm>
#include <cmath>
#include "scene.hpp"

Texture2D BambooBombAttack::explosionTexture{};
bool BambooBombAttack::explosionTextureLoaded = false;
int BambooBombAttack::explosionTextureUsers = 0;

// --- BambooTripleAttack: rapid-fire mode triggered by three same bamboo tiles ---
void BambooTripleAttack::trigger(UpdateContext &uc)
{
    if (this->cooldownRemaining > 0.0f)
        return;

    this->effectRemaining = effectDuration;
    this->cooldownRemaining = cooldownDuration;
}

void BambooTripleAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();

    if (this->cooldownRemaining > 0.0f)
    {
        this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);
    }

    if (this->effectRemaining > 0.0f)
    {
        this->effectRemaining = fmaxf(0.0f, this->effectRemaining - delta);
    }
}

float BambooTripleAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 1.0f;
    return Clamp(1.0f - (this->cooldownRemaining / cooldownDuration), 0.0f, 1.0f);
}

float BambooTripleAttack::getReducedCooldown() const
{
    return this->isActive() ? reducedCooldown : normalCooldown;
}

// --- BasicTileAttack: simple horizontal shooting ---
void BasicTileAttack::spawnProjectile(UpdateContext &uc)
{
    if (!uc.scene || !this->spawnedBy)
        return;

    // Check cooldown
    if (this->cooldownRemaining > 0.0f)
        return;

    Vector3 forward = {0.0f, 0.0f, -1.0f};
    Vector3 spawnPos = this->spawnedBy->pos();
    
    // Get tile stats from selected tile
    float tileDamage = projectileDamage;
    float tileCooldown = cooldownDuration;
    
    if (uc.uiManager && uc.player)
    {
        int selectedIndex = uc.uiManager->muim.getSelectedTileIndex();
        auto &tiles = uc.player->hand.getTiles();
        if (selectedIndex >= 0 && selectedIndex < (int)tiles.size())
        {
            const TileStats &stats = tiles[selectedIndex].stat;
            tileDamage = stats.damage;
            tileCooldown = stats.getCooldownDuration(cooldownDuration);
        }
    }

    // Get firing direction from spawner
    if (this->spawnedBy->category() == ENTITY_PLAYER)
    {
        Me *player = static_cast<Me *>(this->spawnedBy);
        const Camera &camera = player->getCamera();
        // Get the actual look direction from camera (keep full 3D direction)
        forward = Vector3Subtract(camera.target, camera.position);
        if (Vector3LengthSqr(forward) < 0.0001f)
        {
            forward = {0.0f, 0.0f, -1.0f};
        }
        forward = Vector3Normalize(forward);
        spawnPos.y += 1.6f; // spawn at eye level
    }
    else if (this->spawnedBy->category() == ENTITY_ENEMY)
    {
        Enemy *enemy = static_cast<Enemy *>(this->spawnedBy);
        forward = enemy->dir();
        if (Vector3LengthSqr(forward) < 0.0001f)
        {
            forward = {0.0f, 0.0f, -1.0f};
        }
        forward = Vector3Normalize(forward);
        spawnPos.y += 1.0f;
    }

    Vector3 velocity = Vector3Scale(forward, shootSpeed);

    // Create projectile object with same proportions as enemy tiles (44, 60, 30) but smaller
    Vector3 tileSize = Vector3Scale({44.0f, 60.0f, 30.0f}, projectileSize);
    Object body(tileSize, spawnPos);
    body.setRotationFromForward(forward);
    body.useTexture = uc.uiManager != nullptr;
    if (uc.uiManager)
    {
        TileType tileType = TileType::BAMBOO_1;
        if (uc.player)
        {
            tileType = uc.uiManager->muim.getSelectedTile(uc.player->hand);
        }
        body.texture = &uc.uiManager->muim.getSpriteSheet();
        body.sourceRect = uc.uiManager->muim.getTile(tileType);
        body.tint = WHITE;
    }
    body.UpdateOBB();

    TileType tile = TileType::BAMBOO_1;
    if (uc.uiManager && uc.player)
    {
        tile = uc.uiManager->muim.getSelectedTile(uc.player->hand);
    }
    Projectile projectile(
        spawnPos,
        velocity,
        forward,
        false,
        body,
        FRICTION,
        AIR_DRAG,
        tile);
    
    // Store the damage in the projectile (we'll need to add a field for this)
    projectile.damage = tileDamage;

    this->projectiles.push_back(projectile);

    // Start cooldown (modified by active cooldown modifier and tile stats)
    this->cooldownRemaining = tileCooldown * this->activeCooldownModifier;

    // Apply movement slow to player
    if (this->spawnedBy->category() == ENTITY_PLAYER)
    {
        Me *player = static_cast<Me *>(this->spawnedBy);
        player->applyShootSlow(movementSlowFactor, movementSlowDuration);
    }
}

void BasicTileAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    
    // Update cooldown
    if (this->cooldownRemaining > 0.0f)
    {
        this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);
    }
    
    // Update all projectiles without physics (no gravity, constant velocity)
    for (auto &p : this->projectiles)
    {
        // Manual position update with constant velocity
        Vector3 newPos = p.pos();
        newPos.x += p.vel().x * delta;
        newPos.y += p.vel().y * delta;
        newPos.z += p.vel().z * delta;
        p.setPosition(newPos);
        
        // Add horizontal spin rotation (like a frisbee/shuriken)
        static float spinAngle = 0.0f;
        spinAngle += horizontalSpinSpeed * delta;
        if (spinAngle > 360.0f) spinAngle -= 360.0f;
        
        // Rotate around the Y-axis (horizontal spin)
        Quaternion spinRotation = QuaternionFromAxisAngle({0.0f, 1.0f, 0.0f}, spinAngle * DEG2RAD);
        p.obj().setRotation(spinRotation);
        p.obj().UpdateOBB();
    }

    // Check for collisions and remove projectiles that hit something
    auto shouldRemove = [&](Projectile &p) -> bool
    {
        // Check if hit ground
        if (p.pos().y <= 0.1f)
        {
            return true;
        }

        // Check collision with static objects
        if (uc.scene)
        {
            const auto worldHits = Object::collided(p.obj(), uc.scene);
            for (const auto &hit : worldHits)
            {
                if (hit.collided && hit.with == nullptr) // hit static geometry
                {
                    return true;
                }
            }

            // Check collision with enemies
            for (Entity *entity : uc.scene->em.getEntities())
            {
                if (!entity || entity->category() != ENTITY_ENEMY)
                    continue;
                Enemy *enemy = static_cast<Enemy *>(entity);
                CollisionResult result = Object::collided(p.obj(), enemy->obj());
                if (result.collided)
                {
                    // Deal damage to enemy using projectile's damage value
                    DamageResult damage(p.damage, result);
                    uc.scene->em.damage(enemy, damage, uc);
                    return true; // remove projectile
                }
            }
        }

        return false;
    };

    this->projectiles.erase(
        std::remove_if(this->projectiles.begin(), this->projectiles.end(), shouldRemove),
        this->projectiles.end());
}

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

// --- BambooBombAttack (renamed from DotBombAttack) --------------------------------------------------------------------

BambooBombAttack::BambooBombAttack(Entity *_spawnedBy) : AttackController(_spawnedBy)
{
    retainExplosionTexture();
}

BambooBombAttack::~BambooBombAttack()
{
    releaseExplosionTexture();
}

bool BambooBombAttack::trigger(UpdateContext &uc, TileType tile)
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

void BambooBombAttack::retainExplosionTexture()
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

void BambooBombAttack::releaseExplosionTexture()
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

void BambooBombAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);
    for (auto &bomb : bombs)
    {
        if (!bomb.exploded)
        {
            bomb.flightTimeRemaining = fmaxf(0.0f, bomb.flightTimeRemaining - delta);
            bomb.projectile.UpdateBody(uc);

            // Apply heavy end-over-end tumbling rotation
            bomb.tumbleRotation += tumbleSpeed * delta;
            if (bomb.tumbleRotation > 360.0f) bomb.tumbleRotation -= 360.0f;
            
            // Get current forward direction and apply pitch rotation
            Vector3 forward = bomb.projectile.dir();
            Vector3 right = Vector3CrossProduct({0.0f, 1.0f, 0.0f}, forward);
            if (Vector3LengthSqr(right) < 0.0001f) right = {1.0f, 0.0f, 0.0f};
            right = Vector3Normalize(right);
            
            // Tumble around the right axis (end-over-end)
            Quaternion tumbleQuat = QuaternionFromAxisAngle(right, bomb.tumbleRotation * DEG2RAD);
            bomb.projectile.obj().setRotation(tumbleQuat);
            bomb.projectile.obj().UpdateOBB();

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

float BambooBombAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 1.0f;
    return Clamp(1.0f - (this->cooldownRemaining / cooldownDuration), 0.0f, 1.0f);
}

std::vector<Entity *> BambooBombAttack::getEntities()
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

std::vector<Object *> BambooBombAttack::obj()
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

void BambooBombAttack::startExplosion(Bomb &bomb, const Vector3 &origin, UpdateContext &uc)
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

void BambooBombAttack::applyExplosionEffects(const Vector3 &origin, UpdateContext &uc)
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
            uc.scene->em.damage(enemy, damage, uc);
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

void BambooBombAttack::updateExplosionBillboard(Bomb &bomb, UpdateContext &uc, float normalizedProgress)
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
            this->mode = MODE_IDLE;
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
            this->mode = MODE_IDLE;
            this->count = 0;
        }
    }
    break;
    default:
        // MODE_IDLE etc. nothing special
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

    TileType tile = TileType::BAMBOO_1;
    if (uc.uiManager && uc.player)
    {
        tile = uc.uiManager->muim.getSelectedTile(uc.player->hand);
    }
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
        uc.scene->em.damage(enemy, damage, uc);
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
        return 1.0f;
    return Clamp(1.0f - (this->cooldownRemaining / cooldownDuration), 0.0f, 1.0f);
}

void MeleePushAttack::initializeTileIndicator(UpdateContext &uc)
{
    if (!this->spawnedBy || this->spawnedBy->category() != ENTITY_PLAYER)
        return;
    if (!uc.uiManager)
        return;

    Texture2D &sheet = uc.uiManager->muim.getSpriteSheet();
    TileType tile = TileType::BAMBOO_1;
    if (uc.uiManager && uc.player)
    {
        tile = uc.uiManager->muim.getSelectedTile(uc.player->hand);
    }

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
        return 1.0f;
    return Clamp(1.0f - (this->cooldownRemaining / dashCooldown), 0.0f, 1.0f);
}

bool DragonClawAttack::tweakModeEnabled = false;
int DragonClawAttack::tweakSelectedCombo = 0;

DragonClawAttack::DragonClawAttack(Entity *_spawnedBy) : AttackController(_spawnedBy)
{
    this->resetArcDefaults();
    this->debugArcPoints.resize(arcDebugSamples);
    // Attempt to load persisted arcs; fall back to defaults if missing/invalid
    this->loadArcCurves();
}

// --- DragonClawAttack: 3-phase claw swipe animation with arc motion + tweak helper ---
void DragonClawAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    if (this->spawnedBy && this->spawnedBy->category() == ENTITY_PLAYER)
    {
        handleTweakHotkeys();
    }
    
    // Update cooldown
    if (cooldownRemaining > 0.0f)
    {
        cooldownRemaining = fmaxf(0.0f, cooldownRemaining - delta);
    }
    
    // Update combo timer
    if (comboCount > 0)
    {
        comboTimer -= delta;
        if (comboTimer <= 0.0f)
        {
            // Reset combo
            comboCount = 0;
            comboTimer = 0.0f;
        }
    }
    
    Vector3 forward = lastForward;
    Vector3 right = lastRight;
    Vector3 basePos = lastBasePos;

    if (this->spawnedBy)
    {
        forward = {0.0f, 0.0f, -1.0f};
        right = {1.0f, 0.0f, 0.0f};
        basePos = this->spawnedBy->pos();

        if (this->spawnedBy->category() == ENTITY_PLAYER)
        {
            Me *player = static_cast<Me *>(this->spawnedBy);
            const Camera &camera = player->getCamera();
            forward = Vector3Subtract(camera.target, camera.position);
            if (Vector3LengthSqr(forward) < 0.0001f)
                forward = {0.0f, 0.0f, -1.0f};
            forward = Vector3Normalize(forward);
        }
        else if (this->spawnedBy->category() == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(this->spawnedBy);
            forward = enemy->dir();
            if (Vector3LengthSqr(forward) < 0.0001f)
                forward = {0.0f, 0.0f, -1.0f};
            forward = Vector3Normalize(forward);
        }

        right = Vector3CrossProduct({0.0f, 1.0f, 0.0f}, forward);
        right = Vector3Normalize(right);

        lastForward = forward;
        lastRight = right;
        lastBasePos = basePos;

        if (tweakModeEnabled)
        {
            refreshDebugArc(forward, right, basePos);
        }
    }

    // Update and clean up active slashes with 3-phase animation
    for (auto &slash : activeSlashes)
    {
        slash.lifetime -= delta;
        slash.animationProgress = Clamp(slash.animationProgress + delta / attackDuration, 0.0f, 1.0f);
        
        // Update position and rotation based on current animation phase
        slash.spiritTile.pos = getSlashPosition(slash.comboIndex, slash.animationProgress, forward, right, basePos);
        
        // Update slash rotation: face tangent of arc
        Vector3 orientation = getSlashOrientation(slash.comboIndex, slash.animationProgress, forward, right);
        slash.spiritTile.setRotationFromForward(orientation);
        slash.spiritTile.UpdateOBB();
        
        // Check for hits during strike phase
        if (slash.animationProgress >= 0.2f && slash.animationProgress <= 0.7f && !slash.hasHit)
        {
            checkSlashHits(slash, uc);
        }
    }
    
    // Remove expired slashes
    activeSlashes.erase(
        std::remove_if(activeSlashes.begin(), activeSlashes.end(),
            [](const SlashEffect &s) { return s.lifetime <= 0.0f; }),
        activeSlashes.end());
}

void DragonClawAttack::spawnSlash(UpdateContext &uc)
{
    if (cooldownRemaining > 0.0f)
        return;
    
    if (!uc.scene || !this->spawnedBy)
        return;
    
    // Determine forward direction
    Vector3 forward = {0.0f, 0.0f, -1.0f};
    Vector3 right = {1.0f, 0.0f, 0.0f};
    
    if (this->spawnedBy->category() == ENTITY_PLAYER)
    {
        Me *player = static_cast<Me *>(this->spawnedBy);
        const Camera &camera = player->getCamera();
        forward = Vector3Subtract(camera.target, camera.position);
        if (Vector3LengthSqr(forward) < 0.0001f)
        {
            forward = {0.0f, 0.0f, -1.0f};
        }
        forward = Vector3Normalize(forward);
        
        // Calculate right vector
        right = Vector3CrossProduct({0.0f, 1.0f, 0.0f}, forward);
        right = Vector3Normalize(right);
        
        // Player takes a step forward with each swing
        Vector3 stepForward = Vector3Scale(forward, playerStepDistance);
        player->setPosition(Vector3Add(player->pos(), stepForward));
        
        // Add camera shake on swing
        player->addCameraShake(0.1f, attackDuration * 0.5f);
    }
    else if (this->spawnedBy->category() == ENTITY_ENEMY)
    {
        Enemy *enemy = static_cast<Enemy *>(this->spawnedBy);
        forward = enemy->dir();
        if (Vector3LengthSqr(forward) < 0.0001f)
        {
            forward = {0.0f, 0.0f, -1.0f};
        }
        forward = Vector3Normalize(forward);
        right = Vector3CrossProduct({0.0f, 1.0f, 0.0f}, forward);
        right = Vector3Normalize(right);
    }

    lastForward = forward;
    lastRight = right;
    lastBasePos = this->spawnedBy->pos();
    if (tweakModeEnabled)
    {
        refreshDebugArc(forward, right, lastBasePos);
    }
    
    // Create slash effect
    SlashEffect slash;
    slash.comboIndex = comboCount;
    slash.animationProgress = 0.0f;
    slash.lifetime = attackDuration;
    slash.hasHit = false;
    
    // Calculate initial position (windup phase start)
    Vector3 basePos = this->spawnedBy->pos();
    slash.startPos = getSlashPosition(comboCount, 0.0f, forward, right, basePos);
    
    // Create spirit tile object with proper dimensions
    slash.spiritTile.size = {spiritTileWidth, spiritTileHeight, spiritTileThickness};
    slash.spiritTile.pos = slash.startPos;
    slash.spiritTile.setAsBox(slash.spiritTile.size);
    
    // Get initial rotation
    Vector3 orientation = getSlashOrientation(slash.comboIndex, 0.0f, forward, right);
    slash.spiritTile.setRotationFromForward(orientation);
    
    // Set texture with translucent tint
    slash.spiritTile.useTexture = uc.uiManager != nullptr;
    if (uc.uiManager)
    {
        slash.spiritTile.texture = &uc.uiManager->muim.getSpriteSheet();
        TileType selectedTile = uc.uiManager->muim.getSelectedTile(uc.player->hand);
        slash.spiritTile.sourceRect = uc.uiManager->muim.getTile(selectedTile);
        // Translucent white tint (stays translucent throughout)
        unsigned char alpha = (unsigned char)(spiritTileOpacity * 255.0f);
        slash.spiritTile.tint = Color{255, 255, 255, alpha};
    }
    
    slash.spiritTile.UpdateOBB();
    activeSlashes.push_back(slash);
    
    // Update combo
    comboCount = (comboCount + 1) % 3;
    comboTimer = comboResetTime;
    cooldownRemaining = attackCooldown;
}

void DragonClawAttack::checkSlashHits(SlashEffect &slash, UpdateContext &uc)
{
    if (slash.hasHit || !uc.scene)
        return;
    
    // Check collision with enemies in range
    for (Entity *entity : uc.scene->em.getEntities())
    {
        if (!entity || entity->category() != ENTITY_ENEMY)
            continue;
        
        Enemy *enemy = static_cast<Enemy *>(entity);
        CollisionResult result = Object::collided(slash.spiritTile, enemy->obj());
        
        if (result.collided)
        {
            // Deal damage
            DamageResult damage(slashDamage, result);
            uc.scene->em.damage(enemy, damage, uc);
            
            // Apply knockback
            Vector3 delta = Vector3Subtract(enemy->pos(), slash.spiritTile.pos);
            Vector3 pushDir = Vector3Normalize(delta);
            enemy->applyKnockback(Vector3Scale(pushDir, 20.0f), 0.3f, 0.0f);
            
            // Camera shake on hit
            if (this->spawnedBy && this->spawnedBy->category() == ENTITY_PLAYER)
            {
                Me *player = static_cast<Me *>(this->spawnedBy);
                player->addCameraShake(cameraShakeMagnitude, cameraShakeDuration);
            }
            
            slash.hasHit = true;
            break;
        }
    }
}

std::vector<Object *> DragonClawAttack::obj()
{
    std::vector<Object *> ret;
    for (auto &slash : activeSlashes)
    {
        // Only fade opacity in follow-through phase (70-100%)
        float followThruStart = (windupDuration + strikeDuration) / attackDuration;
        float alpha = spiritTileOpacity;
        
        if (slash.animationProgress >= followThruStart)
        {
            // Fade out during follow-through
            float fadeProgress = (slash.animationProgress - followThruStart) / (1.0f - followThruStart);
            alpha = spiritTileOpacity * (1.0f - fadeProgress);
        }
        
        unsigned char alphaVal = (unsigned char)(alpha * 255.0f);
        slash.spiritTile.tint.a = alphaVal;
        ret.push_back(&slash.spiritTile);
    }

    // Debug arc particles for tweak mode
    if (tweakModeEnabled)
    {
        for (auto &p : debugArcPoints)
        {
            ret.push_back(&p);
        }
    }
    return ret;
}

// Calculate slash position using predetermined cubic Bézier arc (local space)
Vector3 DragonClawAttack::getSlashPosition(int comboIndex, float progress, const Vector3 &forward, const Vector3 &right, const Vector3 &basePos) const
{
    int idx = comboIndex % (int)arcCurves.size();
    float arcT = mapProgressToArcT(progress);
    return evalArcPoint(arcCurves[idx], arcT, forward, right, basePos);
}

// Calculate slash orientation so tile faces tangent of the arc
Vector3 DragonClawAttack::getSlashOrientation(int comboIndex, float progress, const Vector3 &forward, const Vector3 &right) const
{
    int idx = comboIndex % (int)arcCurves.size();
    float arcT = mapProgressToArcT(progress);
    Vector3 tangent = evalArcTangent(arcCurves[idx], arcT, forward, right);
    if (Vector3LengthSqr(tangent) < 0.0001f)
        tangent = forward;
    return Vector3Normalize(tangent);
}

// Calculate smooth rotation amount across animation phases
float DragonClawAttack::getRotationAmount(float progress) const
{
    float windupEnd = windupDuration / attackDuration;
    float strikeEnd = (windupDuration + strikeDuration) / attackDuration;
    
    if (progress < windupEnd)
    {
        // Windup phase: ease into windup rotation
        float t = progress / windupEnd;
        return windupRotation * easeInCubic(t);
    }
    else if (progress < strikeEnd)
    {
        // Strike phase: rapid rotation from windup to strike
        float t = (progress - windupEnd) / (strikeEnd - windupEnd);
        return windupRotation + (strikeRotation - windupRotation) * t;
    }
    else
    {
        // Follow-through phase: ease out to final rotation
        float t = (progress - strikeEnd) / (1.0f - strikeEnd);
        return strikeRotation + (followThruRotation - strikeRotation) * easeOutCubic(t);
    }
}

float DragonClawAttack::mapProgressToArcT(float progress) const
{
    // Weight strike phase to cover most of the travel along the arc
    float windupEnd = windupDuration / attackDuration;
    float strikeEnd = (windupDuration + strikeDuration) / attackDuration;

    if (progress < windupEnd)
    {
        float t = progress / windupEnd;
        return 0.15f * easeInCubic(t);
    }
    else if (progress < strikeEnd)
    {
        float t = (progress - windupEnd) / (strikeEnd - windupEnd);
        return 0.15f + 0.70f * t;
    }
    float t = (progress - strikeEnd) / (1.0f - strikeEnd);
    return 0.85f + 0.15f * easeOutCubic(t);
}

Vector3 DragonClawAttack::evalArcPoint(const ArcCurve &curve, float t, const Vector3 &forward, const Vector3 &right, const Vector3 &basePos) const
{
    float u = 1.0f - t;
    float uu = u * u;
    float tt = t * t;
    float uuu = uu * u;
    float ttt = tt * t;

    // Bézier in local space (x=right, y=up, z=forward)
    Vector3 local = Vector3Zero();
    local = Vector3Add(local, Vector3Scale(curve.p0, uuu));
    local = Vector3Add(local, Vector3Scale(curve.p1, 3.0f * uu * t));
    local = Vector3Add(local, Vector3Scale(curve.p2, 3.0f * u * tt));
    local = Vector3Add(local, Vector3Scale(curve.p3, ttt));

    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 world = basePos;
    world = Vector3Add(world, Vector3Scale(right, local.x));
    world = Vector3Add(world, Vector3Scale(up, local.y));
    world = Vector3Add(world, Vector3Scale(forward, local.z));
    return world;
}

Vector3 DragonClawAttack::evalArcTangent(const ArcCurve &curve, float t, const Vector3 &forward, const Vector3 &right) const
{
    float u = 1.0f - t;
    // Derivative of cubic Bézier in local space
    Vector3 p0p1 = Vector3Subtract(curve.p1, curve.p0);
    Vector3 p1p2 = Vector3Subtract(curve.p2, curve.p1);
    Vector3 p2p3 = Vector3Subtract(curve.p3, curve.p2);

    Vector3 local = Vector3Zero();
    local = Vector3Add(local, Vector3Scale(p0p1, 3.0f * u * u));
    local = Vector3Add(local, Vector3Scale(p1p2, 6.0f * u * t));
    local = Vector3Add(local, Vector3Scale(p2p3, 3.0f * t * t));

    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 world = Vector3Zero();
    world = Vector3Add(world, Vector3Scale(right, local.x));
    world = Vector3Add(world, Vector3Scale(up, local.y));
    world = Vector3Add(world, Vector3Scale(forward, local.z));
    return world;
}

void DragonClawAttack::resetArcDefaults()
{
    defaultArcCurves = {
        ArcCurve{ // Combo 1: left-to-right swipe
            { -sideOffset, 0.4f, startDistance },
            { -sideOffset * 0.7f, arcHeight, strikeDistance * 0.35f },
            { sideOffset * 0.2f, arcHeight * 0.6f, strikeDistance * 0.85f },
            { 0.2f, 0.0f, endDistance }
        },
        ArcCurve{ // Combo 2: right-to-left swipe
            { sideOffset, 0.4f, startDistance },
            { sideOffset * 0.7f, arcHeight, strikeDistance * 0.35f },
            { -sideOffset * 0.2f, arcHeight * 0.6f, strikeDistance * 0.85f },
            { -0.2f, 0.0f, endDistance }
        },
        ArcCurve{ // Combo 3: centered downward chop
            { 0.0f, 0.6f, startDistance },
            { 0.35f, arcHeight * 0.9f, strikeDistance * 0.4f },
            { 0.0f, arcHeight * 0.35f, strikeDistance * 0.9f },
            { 0.0f, -0.15f, endDistance + 0.4f }
        }
    };
    arcCurves = defaultArcCurves;
}

void DragonClawAttack::nudgeArc(int comboIndex, const Vector3 &deltaP1, const Vector3 &deltaP2)
{
    int idx = comboIndex % (int)arcCurves.size();
    arcCurves[idx].p1 = Vector3Add(arcCurves[idx].p1, deltaP1);
    arcCurves[idx].p2 = Vector3Add(arcCurves[idx].p2, deltaP2);
}

void DragonClawAttack::nudgeArcPoint(int comboIndex, int pointIndex, const Vector3 &delta)
{
    int idx = comboIndex % (int)arcCurves.size();
    switch (pointIndex)
    {
    case 0: arcCurves[idx].p0 = Vector3Add(arcCurves[idx].p0, delta); break;
    case 1: arcCurves[idx].p1 = Vector3Add(arcCurves[idx].p1, delta); break;
    case 2: arcCurves[idx].p2 = Vector3Add(arcCurves[idx].p2, delta); break;
    case 3: arcCurves[idx].p3 = Vector3Add(arcCurves[idx].p3, delta); break;
    }
}

void DragonClawAttack::handleTweakHotkeys()
{
    // Toggle tweak mode on F2 press (only if SeismicSlam tweak is not active)
    static bool f2WasPressed = false;
    bool f2IsPressed = IsKeyDown(KEY_F2);
    if (f2IsPressed && !f2WasPressed && !SeismicSlamAttack::isTweakModeEnabled())
    {
        tweakModeEnabled = !tweakModeEnabled;
    }
    f2WasPressed = f2IsPressed;
    
    // Tab to switch to SeismicSlam tweak mode
    if (tweakModeEnabled && IsKeyPressed(KEY_TAB))
    {
        tweakModeEnabled = false;
        SeismicSlamAttack::setTweakMode(true);
        return;
    }

    if (!tweakModeEnabled)
        return;

    // Select which arc to edit (1-3)
    if (IsKeyPressed(KEY_ONE)) tweakSelectedCombo = 0;
    if (IsKeyPressed(KEY_TWO)) tweakSelectedCombo = 1;
    if (IsKeyPressed(KEY_THREE)) tweakSelectedCombo = 2;
    
    // Select which control point to edit (Q=p0, W=p1, E=p2, R=p3)
    static int selectedPoint = 1; // Default to p1
    if (IsKeyPressed(KEY_Q)) selectedPoint = 0; // Start point
    if (IsKeyPressed(KEY_W)) selectedPoint = 1; // First control
    if (IsKeyPressed(KEY_E)) selectedPoint = 2; // Second control
    if (IsKeyPressed(KEY_T)) selectedPoint = 3; // End point
    
    if (IsKeyPressed(KEY_R))
    {
        arcCurves = defaultArcCurves;
    }
    if (IsKeyPressed(KEY_F5))
    {
        saveArcCurves();
    }
    if (IsKeyPressed(KEY_F6))
    {
        loadArcCurves();
    }

    float delta = GetFrameTime();
    Vector3 deltaPoint = {0.0f, 0.0f, 0.0f};
    const float sideStep = 2.4f * delta;
    const float heightStep = 2.0f * delta;
    const float forwardStep = 3.0f * delta;

    if (IsKeyDown(KEY_UP))   { deltaPoint.y += heightStep; }
    if (IsKeyDown(KEY_DOWN)) { deltaPoint.y -= heightStep; }
    if (IsKeyDown(KEY_LEFT)) { deltaPoint.x -= sideStep; }
    if (IsKeyDown(KEY_RIGHT)){ deltaPoint.x += sideStep; }
    if (IsKeyDown(KEY_COMMA)){ deltaPoint.z -= forwardStep; }
    if (IsKeyDown(KEY_PERIOD)){ deltaPoint.z += forwardStep; }

    if (Vector3LengthSqr(deltaPoint) > 0.0f)
    {
        nudgeArcPoint(tweakSelectedCombo, selectedPoint, deltaPoint);
    }
}

void DragonClawAttack::refreshDebugArc(const Vector3 &forward, const Vector3 &right, const Vector3 &basePos)
{
    if (!tweakModeEnabled)
        return;

    if (debugArcPoints.size() != arcDebugSamples)
        debugArcPoints.resize(arcDebugSamples);

    const ArcCurve &curve = arcCurves[tweakSelectedCombo % (int)arcCurves.size()];
    for (int i = 0; i < arcDebugSamples; ++i)
    {
        float t = (arcDebugSamples == 1) ? 0.0f : (float)i / (float)(arcDebugSamples - 1);
        Vector3 pos = evalArcPoint(curve, t, forward, right, basePos);
        debugArcPoints[i].pos = pos;
        debugArcPoints[i].setAsSphere(debugParticleRadius);
        debugArcPoints[i].tint = Color{255, 200, 120, 200};
        debugArcPoints[i].useTexture = false;
        debugArcPoints[i].UpdateOBB();
    }
}

bool DragonClawAttack::saveArcCurves() const
{
    std::ofstream out(arcSaveFilename);
    if (!out.is_open())
        return false;

    for (const auto &c : arcCurves)
    {
        out << c.p0.x << ' ' << c.p0.y << ' ' << c.p0.z << ' '
            << c.p1.x << ' ' << c.p1.y << ' ' << c.p1.z << ' '
            << c.p2.x << ' ' << c.p2.y << ' ' << c.p2.z << ' '
            << c.p3.x << ' ' << c.p3.y << ' ' << c.p3.z << '\n';
    }
    return true;
}

bool DragonClawAttack::loadArcCurves()
{
    std::ifstream in(arcSaveFilename);
    if (!in.is_open())
        return false;

    std::array<ArcCurve, 3> loaded = defaultArcCurves;
    for (auto &c : loaded)
    {
        if (!(in >> c.p0.x >> c.p0.y >> c.p0.z
                >> c.p1.x >> c.p1.y >> c.p1.z
                >> c.p2.x >> c.p2.y >> c.p2.z
                >> c.p3.x >> c.p3.y >> c.p3.z))
        {
            return false;
        }
    }
    arcCurves = loaded;
    if (tweakModeEnabled)
    {
        refreshDebugArc(lastForward, lastRight, lastBasePos);
    }
    return true;
}

void DragonClawAttack::applyTweakCamera(const Me &player, Camera &cam)
{
    if (!tweakModeEnabled)
        return;

    Vector2 rot = player.getLookRotation();
    Vector3 forward = {sinf(rot.x), 0.0f, cosf(rot.x)};
    forward = Vector3Normalize(forward);
    Vector3 up = {0.0f, 1.0f, 0.0f};

    Vector3 playerPos = player.pos();
    Vector3 offsetBack = Vector3Scale(forward, -6.0f);
    Vector3 offsetUp = {0.0f, 6.0f, 0.0f};
    cam.position = Vector3Add(Vector3Add(playerPos, offsetBack), offsetUp);
    cam.target = Vector3Add(playerPos, Vector3Scale(forward, 1.5f));
    cam.up = up;
    cam.fovy = 65.0f;
}

void DragonClawAttack::drawTweakHud(const Me &player)
{
    if (!tweakModeEnabled)
        return;

    // Get selected point from static variable in handleTweakHotkeys
    static int displayPoint = 1;
    if (IsKeyPressed(KEY_Q)) displayPoint = 0;
    if (IsKeyPressed(KEY_W)) displayPoint = 1;
    if (IsKeyPressed(KEY_E)) displayPoint = 2;
    if (IsKeyPressed(KEY_T)) displayPoint = 3;
    
    const char* pointNames[] = {"Start (p0)", "Control 1 (p1)", "Control 2 (p2)", "End (p3)"};

    const int baseX = 20;
    int y = 20;
    int line = 18;
    DrawText("Claw Animation Tweak Mode", baseX, y, 20, ORANGE); y += line + 4;
    DrawText(TextFormat("Selected arc: %d | Point: %s", tweakSelectedCombo + 1, pointNames[displayPoint]), baseX, y, 16, YELLOW); y += line;
    DrawText("F2: Toggle tweak mode", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("TAB: Switch to Seismic Slam tweak", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("1/2/3: Pick arc", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("Q/W/E/T: Select point (p0/p1/p2/p3)", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("R: Reset arc", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("Arrow keys: Side/Height", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("</>: Forward/Back control", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("F5: Save arcs", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("F6: Load arcs", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("Left Click: Use attack during tweak", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("Camera set to top-back view", baseX, y, 16, LIGHTGRAY);
}

// --- ArcaneOrbAttack: homing projectile with smooth tracking and sine-wave motion ---
void ArcaneOrbAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    
    // Update cooldown
    if (cooldownRemaining > 0.0f)
    {
        cooldownRemaining = fmaxf(0.0f, cooldownRemaining - delta);
    }
    
    // Update projectiles
    for (auto &orb : activeOrbs)
    {
        if (!orb.active)
            continue;
        
        orb.lifetime += delta;
        
        // Remove if too old
        if (orb.lifetime >= OrbProjectile::maxLifetime)
        {
            orb.active = false;
            continue;
        }
        
        updateOrbMovement(orb, uc, delta);
        checkOrbHits(orb, uc);
    }
    
    // Remove inactive orbs
    activeOrbs.erase(
        std::remove_if(activeOrbs.begin(), activeOrbs.end(),
            [](const OrbProjectile &o) { return !o.active; }),
        activeOrbs.end());
}

void ArcaneOrbAttack::spawnOrb(UpdateContext &uc)
{
    if (cooldownRemaining > 0.0f)
        return;
    
    if (!uc.scene || !this->spawnedBy)
        return;
    
    // Determine forward direction
    Vector3 forward = {0.0f, 0.0f, -1.0f};
    Vector3 spawnPos = this->spawnedBy->pos();
    
    if (this->spawnedBy->category() == ENTITY_PLAYER)
    {
        Me *player = static_cast<Me *>(this->spawnedBy);
        const Camera &camera = player->getCamera();
        forward = Vector3Subtract(camera.target, camera.position);
        if (Vector3LengthSqr(forward) < 0.0001f)
        {
            forward = {0.0f, 0.0f, -1.0f};
        }
        forward = Vector3Normalize(forward);
        spawnPos.y += muzzleHeight;
    }
    else if (this->spawnedBy->category() == ENTITY_ENEMY)
    {
        Enemy *enemy = static_cast<Enemy *>(this->spawnedBy);
        forward = enemy->dir();
        if (Vector3LengthSqr(forward) < 0.0001f)
        {
            forward = {0.0f, 0.0f, -1.0f};
        }
        forward = Vector3Normalize(forward);
        spawnPos.y += 1.0f;
    }
    
    OrbProjectile orb;
    orb.position = spawnPos;
    orb.lastDirection = forward;  // Initial direction
    orb.lifetime = 0.0f;
    orb.sineWavePhase = 0.0f;
    orb.targetEnemy = nullptr;
    orb.active = true;
    
    // Find nearest enemy for homing
    orb.targetEnemy = findNearestEnemy(uc, spawnPos, OrbProjectile::searchRadius);
    if (orb.targetEnemy)
    {
        orb.targetPos = orb.targetEnemy->pos();
    }
    else
    {
        orb.targetPos = Vector3Add(spawnPos, Vector3Scale(forward, 50.0f));
    }
    
    // Create orb object
    orb.orbObj.setAsSphere(orbSize);
    orb.orbObj.pos = orb.position;
    orb.orbObj.useTexture = uc.uiManager != nullptr;
    if (uc.uiManager)
    {
        orb.orbObj.texture = &uc.uiManager->muim.getSpriteSheet();
        TileType selectedTile = uc.uiManager->muim.getSelectedTile(uc.player->hand);
        orb.orbObj.sourceRect = uc.uiManager->muim.getTile(selectedTile);
        orb.orbObj.tint = Color{100, 200, 255, 180}; // Blue tint for magic orb
    }
    orb.orbObj.UpdateOBB();
    
    activeOrbs.push_back(orb);
    cooldownRemaining = cooldownDuration;
}

void ArcaneOrbAttack::updateOrbMovement(OrbProjectile &orb, UpdateContext &uc, float deltaSeconds)
{
    // Update target if enemy is still alive
    if (orb.targetEnemy)
    {
        if (orb.targetEnemy->category() == ENTITY_ENEMY)
        {
            Enemy *enemy = static_cast<Enemy *>(orb.targetEnemy);
            orb.targetPos = enemy->pos();
        }
        else
        {
            orb.targetEnemy = nullptr;
        }
    }
    
    // Find new target if needed
    if (!orb.targetEnemy && uc.scene)
    {
        orb.targetEnemy = findNearestEnemy(uc, orb.position, OrbProjectile::searchRadius);
        if (orb.targetEnemy)
        {
            orb.targetPos = orb.targetEnemy->pos();
        }
    }
    
    // Compute desired direction to target
    Vector3 toTarget = Vector3Subtract(orb.targetPos, orb.position);
    Vector3 targetDir = Vector3Normalize(toTarget);
    
    // Blend between last direction and target direction for smooth tracking
    // trackingBlend = 0.4f means 40% target tracking, 60% inertia from previous direction
    Vector3 blendedDir = Vector3Lerp(
        orb.lastDirection,
        targetDir,
        OrbProjectile::trackingBlend);
    blendedDir = Vector3Normalize(blendedDir);
    
    // Apply sine-wave motion for magical feel (vertical oscillation)
    orb.sineWavePhase += deltaSeconds * OrbProjectile::sineWaveFrequency;
    float verticalSineWave = sinf(orb.sineWavePhase) * OrbProjectile::sineWaveAmplitude;
    
    // Get perpendicular vector for sine wave offset
    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 right = Vector3CrossProduct(blendedDir, up);
    if (Vector3LengthSqr(right) < 0.0001f)
    {
        right = {1.0f, 0.0f, 0.0f};
    }
    right = Vector3Normalize(right);
    
    // Apply sine wave to vertical component
    Vector3 finalDir = blendedDir;
    finalDir.y += verticalSineWave * 0.1f;  // Add vertical oscillation
    finalDir = Vector3Normalize(finalDir);
    
    // Store direction for next frame tracking
    orb.lastDirection = finalDir;
    
    // Update position
    orb.position = Vector3Add(orb.position, Vector3Scale(finalDir, OrbProjectile::baseSpeed * deltaSeconds));
    
    // Update object
    orb.orbObj.pos = orb.position;
    
    // Add rotation to orb for visual interest
    static float orbRotation = 0.0f;
    orbRotation += orbSpinSpeed * deltaSeconds;
    if (orbRotation > 360.0f) orbRotation -= 360.0f;
    Quaternion rot = QuaternionFromEuler(orbRotation * DEG2RAD, orbRotation * DEG2RAD * 0.5f, orbRotation * DEG2RAD * 0.3f);
    orb.orbObj.setRotation(rot);
    
    orb.orbObj.UpdateOBB();
}

void ArcaneOrbAttack::checkOrbHits(OrbProjectile &orb, UpdateContext &uc)
{
    if (!uc.scene)
        return;
    
    // Check collision with enemies
    for (Entity *entity : uc.scene->em.getEntities())
    {
        if (!entity || entity->category() != ENTITY_ENEMY)
            continue;
        
        Enemy *enemy = static_cast<Enemy *>(entity);
        CollisionResult result = Object::collided(orb.orbObj, enemy->obj());
        
        if (result.collided)
        {
            // Deal damage
            DamageResult damage(OrbProjectile::damage, result);
            uc.scene->em.damage(enemy, damage, uc);
            
            // Apply knockback
            Vector3 delta = Vector3Subtract(enemy->pos(), orb.position);
            Vector3 pushDir = Vector3Normalize(delta);
            enemy->applyKnockback(Vector3Scale(pushDir, 15.0f), 0.2f, 0.0f);
            
            orb.active = false;
            break;
        }
    }
    
    // Check if hit ground
    if (orb.position.y <= 0.1f)
    {
        orb.active = false;
    }
}

Entity *ArcaneOrbAttack::findNearestEnemy(UpdateContext &uc, const Vector3 &position, float searchRadius) const
{
    if (!uc.scene)
        return nullptr;
    
    Entity *nearest = nullptr;
    float minDistSq = searchRadius * searchRadius;
    
    for (Entity *entity : uc.scene->em.getEntities())
    {
        if (!entity || entity->category() != ENTITY_ENEMY)
            continue;
        
        float distSq = Vector3DistanceSqr(position, entity->pos());
        if (distSq < minDistSq)
        {
            minDistSq = distSq;
            nearest = entity;
        }
    }
    
    return nearest;
}

std::vector<Object *> ArcaneOrbAttack::obj()
{
    std::vector<Object *> ret;
    for (auto &orb : activeOrbs)
    {
        if (orb.active)
        {
            ret.push_back(&orb.orbObj);
        }
    }
    return ret;
}

// ============================================================================
// GravityWellAttack Implementation
// ============================================================================

bool GravityWellAttack::trigger(UpdateContext &uc)
{
    if (this->cooldownRemaining > 0.0f || this->projectile.active || this->activeWell.active)
        return false;
    if (!this->spawnedBy || !uc.player)
        return false;

    Vector3 forward = {0.0f, 0.0f, -1.0f};
    Vector3 spawnPos = this->spawnedBy->pos();
    if (this->spawnedBy->category() == ENTITY_PLAYER)
    {
        const Camera &cam = uc.player->getCamera();
        forward = Vector3Subtract(cam.target, cam.position);
    }
    if (Vector3LengthSqr(forward) < 0.0001f)
        forward = {0.0f, 0.0f, -1.0f};
    forward = Vector3Normalize(forward);

    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 wiggleAxis = Vector3CrossProduct(forward, up);
    if (Vector3LengthSqr(wiggleAxis) < 1e-4f)
        wiggleAxis = {1.0f, 0.0f, 0.0f};
    wiggleAxis = Vector3Normalize(wiggleAxis);

    spawnPos = Vector3Add(spawnPos, Vector3Scale(forward, 2.2f));
    spawnPos.y += 1.2f;

    this->projectile.active = true;
    this->projectile.position = spawnPos;
    this->projectile.velocity = Vector3Scale(forward, flightSpeed);
    this->projectile.velocity.y += flightLift;
    this->projectile.wiggleAxis = wiggleAxis;
    this->projectile.sinePhase = 0.0f;
    this->projectile.visual.setAsSphere(projectileRadius);
    this->projectile.visual.pos = spawnPos;
    this->projectile.visual.tint = Color{8, 8, 12, 230};
    this->projectile.visual.useTexture = false;
    this->projectile.visual.texture = nullptr;
    this->projectile.visual.setVisible(true);
    this->projectile.visual.UpdateOBB();

    this->cooldownRemaining = cooldownDuration;
    return true;
}

void GravityWellAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    if (this->cooldownRemaining > 0.0f)
        this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);

    // Flight phase: slow projectile with sine wiggle and light gravity
    if (this->projectile.active)
    {
        this->projectile.velocity.y -= projectileGravity * delta;
        this->projectile.sinePhase += flightSineFrequency * delta;
        float wiggle = sinf(this->projectile.sinePhase) * flightSineAmplitude;
        Vector3 wiggleVel = Vector3Scale(this->projectile.wiggleAxis, wiggle);
        Vector3 step = Vector3Add(this->projectile.velocity, wiggleVel);
        this->projectile.position = Vector3Add(this->projectile.position, Vector3Scale(step, delta));
        this->projectile.visual.pos = this->projectile.position;
        this->projectile.visual.UpdateOBB();

        if (this->projectile.position.y <= projectileRadius)
        {
            this->projectile.position.y = projectileRadius;
            this->projectile.visual.pos = this->projectile.position;
            this->projectile.visual.UpdateOBB();
            this->projectile.active = false;
            this->projectile.visual.setVisible(false);

            this->activeWell.active = true;
            this->activeWell.opening = true;
            this->activeWell.collapsing = false;
            this->activeWell.lifetime = wellDuration;
            this->activeWell.openTimer = openingDuration;
            this->activeWell.collapseTimer = collapseDuration;
            this->activeWell.currentRadius = projectileRadius;

            this->activeWell.core.setAsSphere(projectileRadius * 0.7f);
            this->activeWell.core.pos = this->projectile.position;
            this->activeWell.core.tint = Color{8, 8, 12, 240};
            this->activeWell.core.useTexture = false;
            this->activeWell.core.texture = nullptr;
            this->activeWell.core.setVisible(true);
            this->activeWell.core.UpdateOBB();

            this->activeWell.outerRing.setVisible(false);
            this->activeWell.innerRing.setVisible(false);

            if (uc.scene)
            {
                uc.scene->particles.spawnRing(this->projectile.position, projectileRadius * 1.4f, 24, Color{120, 60, 190, 220}, 1.5f, true);
                uc.scene->particles.spawnRing(this->projectile.position, projectileRadius * 0.9f, 18, Color{80, 30, 140, 210}, 1.1f, true);
            }
        }
    }

    if (!this->activeWell.active)
        return;

    this->activeWell.lifetime = fmaxf(0.0f, this->activeWell.lifetime - delta);

    if (this->activeWell.opening)
    {
        this->activeWell.openTimer = fmaxf(0.0f, this->activeWell.openTimer - delta);
        float t = 1.0f - (this->activeWell.openTimer / openingDuration);
        t = Clamp(t, 0.0f, 1.0f);
        float eased = t * t * (3.0f - 2.0f * t); // smoothstep
        this->activeWell.currentRadius = Lerp(projectileRadius, pullRadius, eased);

        float innerR = Lerp(projectileRadius * 0.6f, suppressRadius, eased);
        this->activeWell.core.setAsSphere(Lerp(projectileRadius * 0.7f, coreRadius, eased));
        this->activeWell.outerRing.size = {this->activeWell.currentRadius * 2.0f, horizonHeight, this->activeWell.currentRadius * 2.0f};
        this->activeWell.innerRing.size = {innerR * 2.0f, horizonHeight * 0.7f, innerR * 2.0f};

        if (this->activeWell.openTimer <= 0.0f)
            this->activeWell.opening = false;
    }

    if (this->activeWell.lifetime <= this->activeWell.collapseTimer)
    {
        this->activeWell.collapsing = true;
    }

    float collapseT = this->activeWell.collapsing ? Clamp(this->activeWell.lifetime / this->activeWell.collapseTimer, 0.0f, 1.0f) : 1.0f;
    float ringPulse = 1.0f + 0.12f * sinf(GetTime() * 9.0f);
    float shownRadius = this->activeWell.currentRadius * collapseT * ringPulse;

    this->activeWell.core.tint.a = (unsigned char)Clamp(200.0f * collapseT, 0.0f, 200.0f);
    this->activeWell.core.UpdateOBB();

    Vector3 center = this->activeWell.core.pos;

    // Suction logic and suppression
    if (uc.scene)
    {
        for (Entity *entity : uc.scene->em.getEntities())
        {
            if (!entity || entity->category() != ENTITY_ENEMY)
                continue;
            Enemy *enemy = static_cast<Enemy *>(entity);
            Vector3 deltaPos = Vector3Subtract(center, enemy->pos());
            deltaPos.y = 0.0f; // keep pull planar to avoid launch
            float distSq = Vector3LengthSqr(deltaPos);
            if (distSq > pullRadius * pullRadius)
                continue;

            float dist = sqrtf(fmaxf(distSq, 0.0001f));
            Vector3 dir = Vector3Scale(deltaPos, 1.0f / dist);
            
            // Planar velocity (horizontal only)
            Vector3 planarVel{enemy->vel().x, 0.0f, enemy->vel().z};
            
            // Component of velocity along pull direction
            float alongPull = Vector3DotProduct(planarVel, dir);
            
            // If moving away from center, clamp to stop outward motion
            if (alongPull < 0.0f)
            {
                // Extract component perpendicular to pull direction
                Vector3 perpComponent = Vector3Subtract(planarVel, Vector3Scale(dir, alongPull));
                // Keep only perpendicular motion, remove outward motion
                planarVel = perpComponent;
            }
            
            // Apply pull force always toward center
            float strength = pullStrength * (1.0f - dist / pullRadius);
            planarVel = Vector3Add(planarVel, Vector3Scale(dir, strength * delta));
            
            // Clamp horizontal speed
            float planarSpeed = Vector3Length(planarVel);
            float maxHVel = 15.0f;
            if (planarSpeed > maxHVel)
            {
                planarVel = Vector3Scale(planarVel, maxHVel / planarSpeed);
            }
            
            // Reconstruct full velocity
            Vector3 vel = enemy->vel();
            vel.x = planarVel.x;
            vel.z = planarVel.z;
            vel.y = fminf(vel.y, 0.0f); // avoid upward kick
            enemy->setVelocity(vel);

            if (dist <= suppressRadius)
            {
                enemy->applyStun(suppressStunDuration);
            }

            if (GetRandomValue(0, 100) < 30)
            {
                uc.scene->particles.spawnDirectional(enemy->pos(), dir, 2, Color{140, 80, 190, 230}, 9.0f, 0.25f);
            }
        }

        // Ambient lines collapsing into the singularity
        if (GetRandomValue(0, 100) < 65)
        {
            float angle = ((float)GetRandomValue(0, 360)) * DEG2RAD;
            float radius = pullRadius * ((float)GetRandomValue(40, 100) / 100.0f);
            Vector3 start = {center.x + cosf(angle) * radius, center.y + 0.4f, center.z + sinf(angle) * radius};
            Vector3 dir = Vector3Normalize(Vector3Subtract(center, start));
            uc.scene->particles.spawnDirectional(start, dir, 2, Color{180, 120, 255, 220}, 13.0f, 0.32f);
        }
        if (GetRandomValue(0, 100) < 45)
        {
            float angle = ((float)GetRandomValue(0, 360)) * DEG2RAD;
            float radius = pullRadius * 0.6f;
            Vector3 start = {center.x + cosf(angle) * radius, center.y + 12.0f, center.z + sinf(angle) * radius};
            Vector3 dir = Vector3Normalize(Vector3Subtract(center, start));
            uc.scene->particles.spawnDirectional(start, dir, 2, Color{130, 70, 200, 220}, 16.0f, 0.36f);
        }

        // Soft indicator ring via particles to replace box visual
        if (GetRandomValue(0, 100) < 18)
        {
            uc.scene->particles.spawnRing(center, shownRadius, 22, Color{120, 70, 200, 120}, 0.9f, true);
        }
    }

    if (this->activeWell.lifetime <= 0.0f)
    {
        TraceLog(LOG_INFO, "[GravityWell] attack ended, clearing well state");
        this->activeWell.active = false;
        this->activeWell.core.setVisible(false);
        this->activeWell.outerRing.setVisible(false);
        this->activeWell.innerRing.setVisible(false);
        
        // When well ends, clamp enemy velocities to prevent fling-out
        if (uc.scene)
        {
            for (Entity *entity : uc.scene->em.getEntities())
            {
                if (!entity || entity->category() != ENTITY_ENEMY)
                    continue;
                Enemy *enemy = static_cast<Enemy *>(entity);
                Vector3 vel = enemy->vel();
                
                // Reduce horizontal speed to prevent outward fling
                float hspeed = Vector3Length({vel.x, 0.0f, vel.z});
                if (hspeed > 8.0f)
                {
                    float scale = 8.0f / hspeed;
                    vel.x *= scale;
                    vel.z *= scale;
                    TraceLog(LOG_INFO, "[GravityWell] clamping fling velocity: enemy=%p hspeed_was=%.2f now=%.2f", (void*)enemy, hspeed, 8.0f);
                    enemy->setVelocity(vel);
                }
            }
        }
    }
}

std::vector<Object *> GravityWellAttack::obj()
{
    std::vector<Object *> out;
    if (this->projectile.active)
    {
        out.push_back(&this->projectile.visual);
    }
    if (this->activeWell.active)
    {
        out.push_back(&this->activeWell.core);
    }
    return out;
}

float GravityWellAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 1.0f;
    return Clamp(1.0f - (this->cooldownRemaining / cooldownDuration), 0.0f, 1.0f);
}

// ============================================================================
// ChainLightningAttack Implementation
// ============================================================================

Entity *ChainLightningAttack::findPrimaryTarget(UpdateContext &uc, Vector3 camPos, Vector3 camForward) const
{
    if (!uc.scene)
        return nullptr;

    Entity *best = nullptr;
    float bestProj = maxRange;
    for (Entity *entity : uc.scene->em.getEntities())
    {
        if (!entity || entity->category() != ENTITY_ENEMY)
            continue;
        Vector3 toEnemy = Vector3Subtract(entity->pos(), camPos);
        float proj = Vector3DotProduct(toEnemy, camForward);
        if (proj < 0.0f || proj > maxRange)
            continue;

        Vector3 along = Vector3Scale(camForward, proj);
        Vector3 perpendicular = Vector3Subtract(toEnemy, along);
        float offset = Vector3Length(perpendicular);
        if (offset > 2.2f)
            continue;

        if (proj < bestProj)
        {
            bestProj = proj;
            best = entity;
        }
    }
    return best;
}

std::vector<Entity *> ChainLightningAttack::findSecondaryTargets(UpdateContext &uc, Entity *primary) const
{
    std::vector<Entity *> candidates;
    if (!uc.scene || !primary)
        return candidates;

    for (Entity *entity : uc.scene->em.getEntities())
    {
        if (!entity || entity == primary || entity->category() != ENTITY_ENEMY)
            continue;
        float dist = Vector3Distance(entity->pos(), primary->pos());
        if (dist <= chainRadius)
            candidates.push_back(entity);
    }

    std::sort(candidates.begin(), candidates.end(), [primary](Entity *a, Entity *b)
              { return Vector3Distance(a->pos(), primary->pos()) < Vector3Distance(b->pos(), primary->pos()); });

    if (candidates.size() > 3)
        candidates.resize(3);
    return candidates;
}

void ChainLightningAttack::applyDamageAndStun(Entity *target, float damage, UpdateContext &uc)
{
    if (!target || target->category() != ENTITY_ENEMY || !uc.scene)
        return;
    Enemy *enemy = static_cast<Enemy *>(target);
    int healthBefore = enemy->getHealth();
    Vector3 enemyPos = enemy->pos();
    TraceLog(LOG_INFO, "[ChainLightning] applyDamageAndStun: dmg=%.1f to enemy=%p health_before=%d", damage, (void*)enemy, healthBefore);
    
    CollisionResult cr{};
    DamageResult d(damage, cr);
    uc.scene->em.damage(enemy, d, uc);
    
    // Only apply stun and particles if enemy is still alive
    if (healthBefore - (int)damage > 0)
    {
        enemy->applyStun(stunDuration);
        enemy->applyElectrocute(stunDuration);
        // Hit particles (white + light blue burst)
        uc.scene->particles.spawnExplosion(enemyPos, 14, Color{210, 240, 255, 230}, 0.18f, 5.0f, 0.7f);
        uc.scene->particles.spawnExplosion(enemyPos, 10, Color{140, 200, 255, 220}, 0.12f, 3.5f, 0.55f);
        uc.scene->particles.spawnRing(enemyPos, 2.2f, 16, Color{180, 220, 255, 200}, 2.5f, true);
    }
    else
    {
        TraceLog(LOG_WARNING, "[ChainLightning] enemy killed by damage, skipping stun and particles");
    }
}

void ChainLightningAttack::rebuildBoltGeometry(Bolt &bolt)
{
    bolt.points.clear();
    bolt.segments.clear();
    bolt.points.push_back(bolt.start);

    Vector3 forward = Vector3Subtract(bolt.end, bolt.start);
    float dist = Vector3Length(forward);
    Vector3 up = {0.0f, 1.0f, 0.0f};
    if (dist < 0.001f)
        dist = 0.001f;
    Vector3 dir = Vector3Scale(forward, 1.0f / dist);
    Vector3 right = Vector3CrossProduct(dir, up);
    if (Vector3LengthSqr(right) < 1e-4f)
        right = {1.0f, 0.0f, 0.0f};
    right = Vector3Normalize(right);
    up = Vector3Normalize(Vector3CrossProduct(right, dir));

    int joints = (int)Clamp((dist * 0.6f), (float)minSegments, (float)maxSegments);
    for (int i = 1; i <= joints; ++i)
    {
        float t = (float)i / (float)(joints + 1);
        Vector3 basePoint = Vector3Lerp(bolt.start, bolt.end, t);

        float phase = ((float)GetRandomValue(0, 100) / 100.0f) * PI * 2.0f;
        float radial = jitterAmount * (0.45f + (float)GetRandomValue(0, 60) / 100.0f);
        Vector3 offset = Vector3Add(Vector3Scale(right, cosf(phase) * radial), Vector3Scale(up, sinf(phase) * radial));
        bolt.points.push_back(Vector3Add(basePoint, offset));
    }
    bolt.points.push_back(bolt.end);

    for (size_t i = 0; i + 1 < bolt.points.size(); ++i)
    {
        Vector3 a = bolt.points[i];
        Vector3 b = bolt.points[i + 1];
        Vector3 segDir = Vector3Subtract(b, a);
        float segLen = Vector3Length(segDir);
        if (segLen < 0.001f)
            continue;
        segDir = Vector3Scale(segDir, 1.0f / segLen);
        Vector3 mid = Vector3Scale(Vector3Add(a, b), 0.5f);

        float thicknessJitter = segmentThickness * (1.0f + ((float)GetRandomValue(-15, 15) / 100.0f));

        Object core({thicknessJitter, thicknessJitter, segLen * 1.02f}, mid);
        core.setRotationFromForward(segDir);
        core.tint = Color{200, 240, 255, 235};
        core.useTexture = false;
        core.UpdateOBB();
        bolt.segments.push_back(core);

        Object glow({segmentGlowThickness, segmentGlowThickness, segLen * 1.05f}, mid);
        glow.setRotationFromForward(segDir);
        glow.tint = Color{90, 180, 255, 140};
        glow.useTexture = false;
        glow.UpdateOBB();
        bolt.segments.push_back(glow);
    }
}

bool ChainLightningAttack::trigger(UpdateContext &uc)
{
    if (this->cooldownRemaining > 0.0f)
    {
        TraceLog(LOG_DEBUG, "[ChainLightning] trigger blocked: cooldown=%.2f", this->cooldownRemaining);
        return false;
    }
    if (!uc.player)
        return false;

    TraceLog(LOG_INFO, "[ChainLightning] trigger() called");
    const Camera &cam = uc.player->getCamera();
    Vector3 camPos = cam.position;
    Vector3 camForward = Vector3Subtract(cam.target, cam.position);
    if (Vector3LengthSqr(camForward) < 0.0001f)
        camForward = {0.0f, 0.0f, -1.0f};
    camForward = Vector3Normalize(camForward);

    Entity *primary = findPrimaryTarget(uc, camPos, camForward);
    if (!primary)
    {
        TraceLog(LOG_INFO, "[ChainLightning] no primary target found");
        return false;
    }
    
    Enemy *primaryEnemy = dynamic_cast<Enemy *>(primary);
    TraceLog(LOG_INFO, "[ChainLightning] found primary target=%p health=%d", (void*)primary, primaryEnemy ? primaryEnemy->getHealth() : -1);

    // Apply damage
    TraceLog(LOG_INFO, "[ChainLightning] applying primary dmg=%.1f", primaryDamage);
    applyDamageAndStun(primary, primaryDamage, uc);
    auto secondaries = findSecondaryTargets(uc, primary);
    TraceLog(LOG_INFO, "[ChainLightning] found %zu secondary targets", secondaries.size());
    for (Entity *e : secondaries)
    {
        Enemy *secEnemy = dynamic_cast<Enemy *>(e);
        TraceLog(LOG_INFO, "[ChainLightning] applying secondary dmg=%.1f to enemy=%p health=%d", secondaryDamage, (void*)e, secEnemy ? secEnemy->getHealth() : -1);
        applyDamageAndStun(e, secondaryDamage, uc);
    }

    if (uc.scene)
    {
        uc.scene->particles.spawnExplosion(primary->pos(), 18, Color{200, 240, 255, 220}, 0.22f, 8.0f, 0.8f);
        uc.scene->particles.spawnRing(primary->pos(), 2.5f, 16, Color{0, 200, 255, 180}, 3.5f, true);
        for (Entity *e : secondaries)
        {
            uc.scene->particles.spawnExplosion(e->pos(), 12, Color{120, 210, 255, 220}, 0.16f, 6.0f, 0.6f);
        }
    }

    // Build bolts
    this->activeBolts.clear();
    Bolt primaryBolt;
    primaryBolt.start = camPos;
    primaryBolt.end = primary->pos();
    primaryBolt.lifetime = boltLifetime;
    rebuildBoltGeometry(primaryBolt);
    this->activeBolts.push_back(primaryBolt);

    for (Entity *e : secondaries)
    {
        Bolt b;
        b.start = primary->pos();
        b.end = e->pos();
        b.lifetime = boltLifetime;
        rebuildBoltGeometry(b);
        this->activeBolts.push_back(b);
    }

    this->cooldownRemaining = cooldownDuration;
    if (uc.player)
        uc.player->addCameraShake(0.35f, 0.18f);
    return true;
}

void ChainLightningAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    if (this->cooldownRemaining > 0.0f)
        this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);

    for (auto &b : this->activeBolts)
    {
        b.lifetime -= delta;
        rebuildBoltGeometry(b);
    }

    this->activeBolts.erase(
        std::remove_if(this->activeBolts.begin(), this->activeBolts.end(), [](const Bolt &b)
                       { return b.lifetime <= 0.0f; }),
        this->activeBolts.end());
}

std::vector<Object *> ChainLightningAttack::obj()
{
    std::vector<Object *> out;
    for (auto &b : this->activeBolts)
    {
        for (auto &seg : b.segments)
            out.push_back(&seg);
    }
    return out;
}

float ChainLightningAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 1.0f;
    return Clamp(1.0f - (this->cooldownRemaining / cooldownDuration), 0.0f, 1.0f);
}

// ============================================================================
// OrbitalShieldAttack Implementation
// ============================================================================

std::vector<OrbitalShieldAttack *> OrbitalShieldAttack::registry;

OrbitalShieldAttack::OrbitalShieldAttack(Entity *_spawnedBy) : AttackController(_spawnedBy)
{
    registry.push_back(this);
}

OrbitalShieldAttack::~OrbitalShieldAttack()
{
    registry.erase(std::remove(registry.begin(), registry.end(), this), registry.end());
}

bool OrbitalShieldAttack::trigger(UpdateContext &uc)
{
    if (this->cooldownRemaining > 0.0f)
        return false;
    if (!this->spawnedBy || this->spawnedBy->category() != ENTITY_PLAYER)
        return false;

    if (this->orbs.empty())
    {
        this->baseAngle = GetTime();
        TileType selected = TileType::DOT_2;
        if (uc.uiManager && uc.player)
            selected = uc.uiManager->muim.getSelectedTile(uc.player->hand);

        this->orbs.clear();
        for (int i = 0; i < maxOrbs; ++i)
        {
            Orb orb;
            orb.angle = this->baseAngle + (2.0f * PI * ((float)i / (float)maxOrbs));
            orb.visual.setAsSphere(0.42f);
            orb.visual.useTexture = uc.uiManager != nullptr;
            if (uc.uiManager)
            {
                orb.visual.texture = &uc.uiManager->muim.getSpriteSheet();
                orb.visual.sourceRect = uc.uiManager->muim.getTile(selected);
            }
            orb.visual.tint = Color{90, 170, 255, 200};
            orb.visual.UpdateOBB();
            this->orbs.push_back(orb);
        }
    }
    else
    {
        // Launch remaining orbs toward crosshair
        Me *player = static_cast<Me *>(this->spawnedBy);
        const Camera &cam = player->getCamera();
        Vector3 forward = Vector3Subtract(cam.target, cam.position);
        if (Vector3LengthSqr(forward) < 0.0001f)
            forward = {0.0f, 0.0f, -1.0f};
        forward = Vector3Normalize(forward);
        for (auto &orb : this->orbs)
        {
            orb.launching = true;
            orb.velocity = Vector3Scale(forward, launchSpeed);
        }
        player->addCameraShake(0.3f, 0.2f);
    }

    this->cooldownRemaining = cooldownDuration;
    return true;
}

void OrbitalShieldAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    if (this->cooldownRemaining > 0.0f)
        this->cooldownRemaining = fmaxf(0.0f, this->cooldownRemaining - delta);

    if (!this->spawnedBy || this->spawnedBy->category() != ENTITY_PLAYER)
    {
        this->orbs.clear();
        return;
    }

    Me *player = static_cast<Me *>(this->spawnedBy);
    Vector3 playerPos = player->pos();

    // Update orbiting / launched orbs
    for (auto &orb : this->orbs)
    {
        if (!orb.launching)
        {
            orb.angle += orbitSpeed * delta;
            float c = cosf(orb.angle);
            float s = sinf(orb.angle);
            Vector3 offset = {c * orbitRadius, orbitHeight, s * orbitRadius};
            orb.visual.pos = Vector3Add(playerPos, offset);
            Vector3 outward = {c, 0.0f, s};
            orb.visual.setRotationFromForward(Vector3Normalize(outward));
            if (uc.scene && GetRandomValue(0, 100) < 24)
            {
                uc.scene->particles.spawnDirectional(orb.visual.pos, outward, 1, Color{120, 190, 255, 180}, 1.5f, 0.15f);
            }
        }
        else
        {
            orb.visual.pos = Vector3Add(orb.visual.pos, Vector3Scale(orb.velocity, delta));
            orb.velocity = Vector3Scale(orb.velocity, 0.985f);

            // Collision vs enemies
            if (uc.scene)
            {
                for (Entity *entity : uc.scene->em.getEntities())
                {
                    if (!entity || entity->category() != ENTITY_ENEMY)
                        continue;
                    Enemy *enemy = static_cast<Enemy *>(entity);
                    CollisionResult result = Object::collided(orb.visual, enemy->obj());
                    if (result.collided)
                    {
                        DamageResult damage(shieldDamage, result);
                        uc.scene->em.damage(enemy, damage, uc);
                        enemy->applyKnockback(Vector3Scale(orb.velocity, 0.2f), 0.35f, 2.5f);
                        orb.launching = false;
                        orb.visual.setVisible(false);
                        orb.velocity = {0.0f, 0.0f, 0.0f};
                        break;
                    }
                }
            }

            float travelDist = Vector3Distance(orb.visual.pos, playerPos);
            if (travelDist > 80.0f || orb.visual.pos.y < -2.0f)
            {
                orb.launching = false;
                orb.visual.setVisible(false);
            }
        }

        orb.visual.UpdateOBB();
    }

    // Remove spent orbs
    this->orbs.erase(
        std::remove_if(this->orbs.begin(), this->orbs.end(), [](const Orb &o)
                       { return !o.launching && !o.visual.isVisible(); }),
        this->orbs.end());
}

std::vector<Object *> OrbitalShieldAttack::obj()
{
    std::vector<Object *> out;
    for (auto &orb : this->orbs)
        out.push_back(&orb.visual);
    return out;
}

float OrbitalShieldAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 1.0f;
    return Clamp(1.0f - (this->cooldownRemaining / cooldownDuration), 0.0f, 1.0f);
}

bool OrbitalShieldAttack::consumeOneShield(Me *player, UpdateContext *uc)
{
    if (!player || this->spawnedBy != player)
        return false;
    for (auto &orb : this->orbs)
    {
        if (!orb.launching)
        {
            orb.visual.setVisible(false);
            orb.launching = false;
            if (uc && uc->scene)
            {
                uc->scene->particles.spawnRing(player->pos(), 2.0f, 12, Color{90, 170, 255, 220}, 2.0f, true);
            }
            player->addCameraShake(0.25f, 0.2f);
            return true;
        }
    }
    return false;
}

bool TryConsumeOrbitalShield(Me *player, DamageResult &dResult)
{
    (void)dResult; // damage amount unused; shield negates fully
    for (auto *shield : OrbitalShieldAttack::registry)
    {
        if (shield && shield->consumeOneShield(player, nullptr))
            return true;
    }
    return false;
}

// ============================================================================
// FanShotAttack Implementation
// ============================================================================

void FanShotAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    cooldownRemaining -= delta;
    if (cooldownRemaining < 0.0f)
        cooldownRemaining = 0.0f;

    // Handle camera recoil animation
    if (recoilActive && uc.player)
    {
        recoilTimer += delta;
        float &currentPitch = uc.player->getLookRotation().y;
        
        if (recoilTimer < recoilKickTime)
        {
            // Fast upward kick phase
            float t = recoilTimer / recoilKickTime;
            float targetPitch = originalPitch - recoilPitchKick;
            float pitchDiff = targetPitch - currentPitch;
                currentPitch += pitchDiff * recoilKickSpeed * delta;
        }
        else if (recoilTimer < recoilDuration)
        {
            // Slower recovery phase
            float recoveryTime = recoilTimer - recoilKickTime;
            float recoveryDuration = recoilDuration - recoilKickTime;
            float t = recoveryTime / recoveryDuration;
            float targetPitch = originalPitch;
            float pitchDiff = targetPitch - currentPitch;
                currentPitch += pitchDiff * recoilRecoverySpeed * delta;
        }
        else
        {
            // Recoil complete
            recoilActive = false;
        }
    }

    // Update all projectiles
    for (auto &proj : projectiles)
    {
        proj.UpdateBody(uc);
    }

    // Remove projectiles that hit enemies or go out of bounds
    auto shouldRemove = [&uc](Projectile &p) -> bool
    {
        // Check if projectile is too low (fell through floor) or too far
        if (p.pos().y < -10.0f || Vector3Distance(p.pos(), {0, 0, 0}) > 200.0f)
            return true;

        // Check collision with enemies
        if (uc.scene)
        {
            for (Entity *entity : uc.scene->em.getEntities())
            {
                if (!entity || entity->category() != ENTITY_ENEMY)
                    continue;
                Enemy *enemy = static_cast<Enemy *>(entity);
                CollisionResult result = Object::collided(p.obj(), enemy->obj());
                if (result.collided)
                {
                    DamageResult damage(p.damage, result);
                    uc.scene->em.damage(enemy, damage, uc);
                    return true; // remove projectile
                }
            }
        }

        return false;
    };

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(), shouldRemove),
        projectiles.end());
}

std::vector<Entity *> FanShotAttack::getEntities()
{
    std::vector<Entity *> ret;
    for (auto &p : projectiles)
    {
        ret.push_back(&p);
    }
    return ret;
}

std::vector<Object *> FanShotAttack::obj()
{
    std::vector<Object *> ret;
    for (auto &p : projectiles)
    {
        ret.push_back(&p.obj());
    }
    return ret;
}

bool FanShotAttack::trigger(UpdateContext &uc)
{
    if (cooldownRemaining > 0.0f)
        return false;

    Vector3 spawnPos = {0.0f, 0.0f, 0.0f};
    Vector3 forward = {0.0f, 0.0f, -1.0f};
    TileType selectedTile = TileType::BAMBOO_1;

    // Get spawn position and direction
    if (this->spawnedBy && this->spawnedBy->category() == ENTITY_PLAYER)
    {
        Me *player = static_cast<Me *>(this->spawnedBy);
        spawnPos = player->pos();
        // Use player look rotation with the same math as MyCamera to match actual view
        Vector2 rot = player->getLookRotation();
        const Vector3 baseForward = {0.0f, 0.0f, -1.0f};
        const Vector3 up = {0.0f, 1.0f, 0.0f};
        Vector3 yawForward = Vector3RotateByAxisAngle(baseForward, up, rot.x);
        Vector3 right = Vector3Normalize(Vector3CrossProduct(yawForward, up));
        float pitchAngle = -rot.y; // MyCamera uses negative pitch for look up/down
        Vector3 pitched = Vector3RotateByAxisAngle(yawForward, right, pitchAngle);
        forward = Vector3Normalize(pitched);

        spawnPos.y += muzzleHeight;

        // Start camera recoil animation
        recoilActive = true;
        recoilTimer = 0.0f;
        originalPitch = uc.player->getLookRotation().y;

        if (uc.uiManager)
        {
            selectedTile = uc.uiManager->muim.getSelectedTile(uc.player->hand);
        }
    }

    // Calculate right vector for horizontal spread
    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 right = Vector3CrossProduct(forward, up);
    if (Vector3LengthSqr(right) < 0.0001f)
    {
        right = {1.0f, 0.0f, 0.0f};
    }
    right = Vector3Normalize(right);

    // Spawn 5 projectiles in horizontal spread
    float halfSpread = spreadAngle * 0.5f * DEG2RAD;
    float angleStep = (spreadCount > 1) ? (spreadAngle * DEG2RAD) / (float)(spreadCount - 1) : 0.0f;

    for (int i = 0; i < spreadCount; ++i)
    {
        // Calculate angle for this projectile (-halfSpread to +halfSpread)
        float angle = -halfSpread + (float)i * angleStep;
        
        // Rotate forward vector around up axis by angle
        Vector3 dir = Vector3RotateByAxisAngle(forward, up, angle);
        dir = Vector3Normalize(dir);

        // Create projectile body
        Object body;
        body.setAsSphere(projectileSize);
        body.pos = spawnPos;
        body.useTexture = uc.uiManager != nullptr;
        if (uc.uiManager)
        {
            body.texture = &uc.uiManager->muim.getSpriteSheet();
            body.sourceRect = uc.uiManager->muim.getTile(selectedTile);
            body.tint = Color{255, 255, 255, 220}; // White-hot tint
        }
        body.UpdateOBB();

        // Create projectile
        Projectile proj(
            spawnPos,
            Vector3Scale(dir, projectileSpeed),
            dir,
            false,
            body,
            1.0f,
            1.0f,
            selectedTile
        );
        proj.damage = projectileDamage;

        projectiles.push_back(proj);
    }

    cooldownRemaining = cooldownDuration;
    return true;
}

float FanShotAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 0.0f;
    return Clamp(1.0f - (cooldownRemaining / cooldownDuration), 0.0f, 1.0f);
}
// ============================================================================
// SeismicSlamAttack Implementation
// ============================================================================

bool SeismicSlamAttack::tweakModeEnabled = false;

SeismicSlamAttack::SeismicSlamAttack(Entity *_spawnedBy)
    : AttackController(_spawnedBy)
{
    // Initialize default arc curve (local space: x=right, y=up, z=forward)
    defaultArcCurve.p0 = {0.0f, 0.0f, 0.0f};                    // Start at origin
    defaultArcCurve.p1 = {0.0f, 3.0f, 2.0f};                    // First control: up and forward
    defaultArcCurve.p2 = {0.0f, arcApexHeight, 5.0f};           // Second control: apex
    defaultArcCurve.p3 = {0.0f, 0.0f, arcForwardDistance};      // End: forward on ground
    
    arcCurve = defaultArcCurve;
    
    // Initialize shockwave ring as a flat box
    shockwaveRing.setAsBox({shockwaveStartRadius * 2.0f, shockwaveHeight, shockwaveStartRadius * 2.0f});
    shockwaveRing.tint = Color{255, 200, 100, 180};
    shockwaveRing.useTexture = false;
    shockwaveRing.setVisible(false);
    
    loadArcCurve();
}

void SeismicSlamAttack::update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    
    // Update cooldown
    if (cooldownRemaining > 0.0f)
    {
        cooldownRemaining -= delta;
        if (cooldownRemaining < 0.0f)
            cooldownRemaining = 0.0f;
    }
    
    // Update state machine
    switch (state)
    {
    case SLAM_IDLE:
        // Nothing to update
        break;
        
    case SLAM_LEAP:
        updateLeap(uc, delta);
        break;
        
    case SLAM_DESCEND:
        // Legacy path; leap now handles full arc and triggers impact directly
        updateDescend(uc, delta);
        break;
        
    case SLAM_IMPACT:
        updateImpact(uc, delta);
        break;
        
    case SLAM_RECOVERY:
        updateRecovery(uc, delta);
        break;
    }
    
    // Update shockwave ring
    if (shockwaveActive)
    {
        shockwaveTimer += delta;
        float progress = shockwaveTimer / impactDuration;
        
        if (progress >= 1.0f)
        {
            shockwaveActive = false;
            shockwaveRing.setVisible(false);
        }
        else
        {
            // Expand ring
            float currentRadius = shockwaveStartRadius + (shockwaveEndRadius - shockwaveStartRadius) * progress;
            shockwaveRing.setAsBox({currentRadius * 2.0f, shockwaveHeight, currentRadius * 2.0f});
            
            // Fade out
            unsigned char alpha = (unsigned char)((1.0f - progress) * 180.0f);
            shockwaveRing.tint.a = alpha;
            
            shockwaveRing.UpdateOBB();
        }
    }
}

bool SeismicSlamAttack::trigger(UpdateContext &uc)
{
    if (cooldownRemaining > 0.0f || state != SLAM_IDLE || tweakModeEnabled)
        return false;
    
    performLeap(uc);
    return true;
}

void SeismicSlamAttack::performLeap(UpdateContext &uc)
{
    if (!uc.player)
        return;
    
    state = SLAM_LEAP;
    stateTimer = 0.0f;
    animationProgress = 0.0f;
    
    // Save player state
    leapStartPos = uc.player->pos();
    savedVelocity = uc.player->vel();
    
    // Store current facing for arc calculation
    const Camera &cam = uc.player->getCamera();
    lastForward = Vector3Subtract(cam.target, cam.position);
    if (Vector3LengthSqr(lastForward) < 0.0001f)
        lastForward = {0.0f, 0.0f, -1.0f};
    lastForward = Vector3Normalize(lastForward);
    lastForward.y = 0.0f; // Keep horizontal
    lastForward = Vector3Normalize(lastForward);
    
    Vector3 up = {0.0f, 1.0f, 0.0f};
    lastRight = Vector3Normalize(Vector3CrossProduct(up, lastForward));
    lastBasePos = leapStartPos;
    
    // Zero out player velocity
    uc.player->setVelocity({0.0f, 0.0f, 0.0f});
}

void SeismicSlamAttack::updateLeap(UpdateContext &uc, float delta)
{
    if (!uc.player)
        return;
    
    stateTimer += delta;
    float linearT = Clamp(stateTimer / leapDuration, 0.0f, 1.0f);
    // Gravity-shaped progress: speeds up at start/end, slows near apex
    float shapedT = linearT + gravityShape * sinf(2.0f * PI * linearT) / (2.0f * PI);
    animationProgress = Clamp(shapedT, 0.0f, 1.0f);

    // Evaluate arc position
    Vector3 arcPos = evalArcPoint(arcCurve, animationProgress, lastForward, lastRight, lastBasePos);
    
    // Check for wall collision before moving
    if (uc.scene)
    {
        Vector3 currentPos = uc.player->pos();
        Vector3 moveDelta = Vector3Subtract(arcPos, currentPos);
        float moveDistance = Vector3Length(moveDelta);
        
        if (moveDistance > 0.001f)
        {
            // Create a temporary collision box at target position
            Object testBox = uc.player->obj();
            testBox.pos = arcPos;
            testBox.UpdateOBB();
            
            // Check collision with scene objects (walls)
            auto collisions = Object::collided(testBox, uc.scene);
            bool hitWall = false;
            for (const auto &col : collisions)
            {
                if (col.collided)
                {
                    hitWall = true;
                    // Stop the leap early and trigger impact
                    performImpact(uc);
                    return;
                }
            }
        }
    }
    
    uc.player->setPosition(arcPos);

    // Calculate tangent for camera pitch (look along motion direction)
    Vector3 tangent = evalArcTangent(arcCurve, animationProgress, lastForward, lastRight);
    tangent = Vector3Normalize(tangent);
    float horizLen = Vector3Length({tangent.x, 0.0f, tangent.z});
    float targetPitch = atan2f(tangent.y, horizLen); // Positive up, negative down
    // During ascent (first half), look upward naturally
    // During descent (second half), smoothly transition to looking straight down
    if (animationProgress > 0.5f)
    {
        float descentProgress = (animationProgress - 0.5f) / 0.5f; // 0 to 1 over second half
        targetPitch = Lerp(targetPitch, -PI * 0.5f, descentProgress);
    }
    updateCameraLook(uc, targetPitch);

    // Trigger impact when arc completes
    if (animationProgress >= 0.999f)
    {
        performImpact(uc);
    }
}

void SeismicSlamAttack::updateDescend(UpdateContext &uc, float delta)
{
    if (!uc.player)
        return;
    
    stateTimer += delta;
    animationProgress = Clamp(stateTimer / descendDuration, 0.0f, 1.0f);
    
    // Simple downward motion
    Vector3 currentPos = uc.player->pos();
    float targetY = 0.0f; // Ground level
    currentPos.y = Lerp(currentPos.y, targetY, animationProgress);
    uc.player->setPosition(currentPos);
    
    // Camera looks down at impact point
    updateCameraLook(uc, -impactLookDownAngle);
    
    // Check for ground impact
    if (currentPos.y <= 0.1f || animationProgress >= 1.0f)
    {
        currentPos.y = 0.0f;
        uc.player->setPosition(currentPos);
        performImpact(uc);
    }
}

void SeismicSlamAttack::performImpact(UpdateContext &uc)
{
    state = SLAM_IMPACT;
    stateTimer = 0.0f;
    
    // Apply camera shake
    if (uc.player)
    {
        uc.player->addCameraShake(cameraShakeMagnitude, cameraShakeDuration);
        // Camera should already be looking down from the dive
        // No need to force set it here
    }
    
    // Impact particles and rings (match Vanguard dive feel)
    if (uc.scene)
    {
        Vector3 pos = uc.player->pos();
        uc.scene->particles.spawnExplosion(pos, 48, RED, 0.4f, 8.0f, 1.0f);
        uc.scene->particles.spawnRing(pos, 4.0f, 32, ColorAlpha(ORANGE, 220), 3.2f, true);
    }

    // Start shockwave visual
    shockwaveActive = true;
    shockwaveTimer = 0.0f;
    shockwaveRing.pos = uc.player->pos();
    shockwaveRing.pos.y = shockwaveHeight * 0.5f;
    shockwaveRing.setAsBox({shockwaveStartRadius * 2.0f, shockwaveHeight, shockwaveStartRadius * 2.0f});
    shockwaveRing.tint = Color{255, 200, 100, 180};
    shockwaveRing.setVisible(true);
    shockwaveRing.UpdateOBB();
    
    // Apply damage to enemies in radius
    applyShockwaveDamage(uc);
}

void SeismicSlamAttack::updateImpact(UpdateContext &uc, float delta)
{
    stateTimer += delta;
    
    if (stateTimer >= impactDuration)
    {
        state = SLAM_RECOVERY;
        stateTimer = 0.0f;
    }
}

void SeismicSlamAttack::updateRecovery(UpdateContext &uc, float delta)
{
    stateTimer += delta;
    
    // Gradually restore camera control with slower speed
    if (!uc.player)
        return;
    
    float &currentPitch = uc.player->getLookRotation().y;
    float targetPitch = 0.0f;
    float pitchDiff = targetPitch - currentPitch;
    currentPitch += pitchDiff * cameraRecoverySpeed * delta;
    
    if (stateTimer >= recoveryDuration)
    {
        state = SLAM_IDLE;
        restoreCameraControl(uc);
        cooldownRemaining = cooldownDuration;
    }
}

void SeismicSlamAttack::applyShockwaveDamage(UpdateContext &uc)
{
    if (!uc.scene || !uc.player)
        return;
    
    Vector3 impactPos = uc.player->pos();
    
    for (Entity *entity : uc.scene->em.getEntities())
    {
        if (!entity || entity->category() != ENTITY_ENEMY)
            continue;
        
        Enemy *enemy = static_cast<Enemy *>(entity);
        float dist = Vector3Distance(impactPos, enemy->pos());
        
        if (dist <= shockwaveEndRadius)
        {
            // Deal damage
            CollisionResult cResult;
            DamageResult damage(slamDamage, cResult);
            uc.scene->em.damage(enemy, damage, uc);
            
            // Apply knockback
            Vector3 delta = Vector3Subtract(enemy->pos(), impactPos);
            delta.y = 0.0f;
            if (Vector3LengthSqr(delta) < 0.0001f)
                delta = {1.0f, 0.0f, 0.0f};
            Vector3 pushDir = Vector3Normalize(delta);
            enemy->applyKnockback(
                Vector3Scale(pushDir, slamKnockback),
                slamKnockbackDuration,
                slamLift
            );
        }
    }
}

void SeismicSlamAttack::updateCameraLook(UpdateContext &uc, float targetPitch)
{
    if (!uc.player)
        return;
    
    float &currentPitch = uc.player->getLookRotation().y;
    float pitchDiff = targetPitch - currentPitch;
    currentPitch += pitchDiff * cameraTransitionSpeed * GetFrameTime();
}

void SeismicSlamAttack::restoreCameraControl(UpdateContext &uc)
{
    // Camera returns to normal - player regains control
    // No special action needed; player input will naturally resume
}

Vector3 SeismicSlamAttack::evalArcPoint(const ArcCurve &curve, float t, const Vector3 &forward, const Vector3 &right, const Vector3 &basePos) const
{
    // Cubic Bezier interpolation
    float oneMinusT = 1.0f - t;
    float b0 = oneMinusT * oneMinusT * oneMinusT;
    float b1 = 3.0f * oneMinusT * oneMinusT * t;
    float b2 = 3.0f * oneMinusT * t * t;
    float b3 = t * t * t;
    
    // Evaluate in local space
    Vector3 localPos = {
        b0 * curve.p0.x + b1 * curve.p1.x + b2 * curve.p2.x + b3 * curve.p3.x,
        b0 * curve.p0.y + b1 * curve.p1.y + b2 * curve.p2.y + b3 * curve.p3.y,
        b0 * curve.p0.z + b1 * curve.p1.z + b2 * curve.p2.z + b3 * curve.p3.z
    };
    
    // Transform to world space
    Vector3 worldPos = basePos;
    worldPos = Vector3Add(worldPos, Vector3Scale(right, localPos.x));
    worldPos = Vector3Add(worldPos, Vector3Scale({0.0f, 1.0f, 0.0f}, localPos.y));
    worldPos = Vector3Add(worldPos, Vector3Scale(forward, localPos.z));
    
    return worldPos;
}

Vector3 SeismicSlamAttack::evalArcTangent(const ArcCurve &curve, float t, const Vector3 &forward, const Vector3 &right) const
{
    // Derivative of cubic Bezier
    float oneMinusT = 1.0f - t;
    float d0 = 3.0f * oneMinusT * oneMinusT;
    float d1 = 6.0f * oneMinusT * t;
    float d2 = 3.0f * t * t;
    
    Vector3 p01 = Vector3Subtract(curve.p1, curve.p0);
    Vector3 p12 = Vector3Subtract(curve.p2, curve.p1);
    Vector3 p23 = Vector3Subtract(curve.p3, curve.p2);
    
    Vector3 localTangent = {
        d0 * p01.x + d1 * p12.x + d2 * p23.x,
        d0 * p01.y + d1 * p12.y + d2 * p23.y,
        d0 * p01.z + d1 * p12.z + d2 * p23.z
    };
    
    // Transform to world space
    Vector3 worldTangent = {0.0f, 0.0f, 0.0f};
    worldTangent = Vector3Add(worldTangent, Vector3Scale(right, localTangent.x));
    worldTangent = Vector3Add(worldTangent, Vector3Scale({0.0f, 1.0f, 0.0f}, localTangent.y));
    worldTangent = Vector3Add(worldTangent, Vector3Scale(forward, localTangent.z));
    
    return worldTangent;
}

float SeismicSlamAttack::getCooldownPercent() const
{
    if (cooldownDuration <= 0.0f)
        return 0.0f;
    return Clamp(1.0f - (cooldownRemaining / cooldownDuration), 0.0f, 1.0f);
}

std::vector<Object *> SeismicSlamAttack::obj()
{
    std::vector<Object *> ret;
    if (shockwaveActive)
    {
        ret.push_back(&shockwaveRing);
    }
    if (tweakModeEnabled && !debugArcPoints.empty())
    {
        for (auto &pt : debugArcPoints)
        {
            ret.push_back(&pt);
        }
    }
    return ret;
}

// ============================================================================
// Tweak Mode for SeismicSlamAttack
// ============================================================================

void SeismicSlamAttack::handleTweakHotkeys()
{
    // Toggle tweak mode on F3 press (only if DragonClaw tweak is not active)
    static bool f3WasPressed = false;
    bool f3IsPressed = IsKeyDown(KEY_F3);
    if (f3IsPressed && !f3WasPressed && !DragonClawAttack::isTweakModeEnabled())
    {
        tweakModeEnabled = !tweakModeEnabled;
    }
    f3WasPressed = f3IsPressed;
    
    // Tab to switch to DragonClaw tweak mode
    if (tweakModeEnabled && IsKeyPressed(KEY_TAB))
    {
        tweakModeEnabled = false;
        DragonClawAttack::setTweakMode(true);
        return;
    }

    if (!tweakModeEnabled)
        return;
    
    // Select which control point to edit (Q=p0, W=p1, E=p2, T=p3)
    static int selectedPoint = 1; // Default to p1
    if (IsKeyPressed(KEY_Q)) selectedPoint = 0;
    if (IsKeyPressed(KEY_W)) selectedPoint = 1;
    if (IsKeyPressed(KEY_E)) selectedPoint = 2;
    if (IsKeyPressed(KEY_T)) selectedPoint = 3;
    
    if (IsKeyPressed(KEY_R))
    {
        resetArcDefaults();
    }
    if (IsKeyPressed(KEY_F5))
    {
        saveArcCurve();
    }
    if (IsKeyPressed(KEY_F6))
    {
        loadArcCurve();
    }

    float delta = GetFrameTime();
    Vector3 deltaPoint = {0.0f, 0.0f, 0.0f};
    const float sideStep = 2.4f * delta;
    const float heightStep = 2.0f * delta;
    const float forwardStep = 3.0f * delta;

    if (IsKeyDown(KEY_UP))    { deltaPoint.y += heightStep; }
    if (IsKeyDown(KEY_DOWN))  { deltaPoint.y -= heightStep; }
    if (IsKeyDown(KEY_LEFT))  { deltaPoint.x -= sideStep; }
    if (IsKeyDown(KEY_RIGHT)) { deltaPoint.x += sideStep; }
    if (IsKeyDown(KEY_COMMA)) { deltaPoint.z -= forwardStep; }
    if (IsKeyDown(KEY_PERIOD)){ deltaPoint.z += forwardStep; }

    if (Vector3LengthSqr(deltaPoint) > 0.0f)
    {
        nudgeArcPoint(selectedPoint, deltaPoint);
    }
}

void SeismicSlamAttack::refreshDebugArc(const Vector3 &forward, const Vector3 &right, const Vector3 &basePos)
{
    if (!tweakModeEnabled)
        return;

    lastForward = forward;
    lastRight = right;
    lastBasePos = basePos;

    if (debugArcPoints.size() != arcDebugSamples)
        debugArcPoints.resize(arcDebugSamples);

    for (int i = 0; i < arcDebugSamples; ++i)
    {
        float t = (arcDebugSamples == 1) ? 0.0f : (float)i / (float)(arcDebugSamples - 1);
        Vector3 pos = evalArcPoint(arcCurve, t, forward, right, basePos);
        debugArcPoints[i].pos = pos;
        debugArcPoints[i].setAsSphere(debugParticleRadius);
        debugArcPoints[i].tint = Color{255, 120, 200, 200};
        debugArcPoints[i].useTexture = false;
        debugArcPoints[i].UpdateOBB();
    }
}

void SeismicSlamAttack::applyTweakCamera(const Me &player, Camera &cam)
{
    if (!tweakModeEnabled)
        return;

    Vector2 rot = player.getLookRotation();
    Vector3 forward = {sinf(rot.x), 0.0f, cosf(rot.x)}; // Horizontal forward (same as DragonClaw)
    forward = Vector3Normalize(forward);
    const Vector3 up = {0.0f, 1.0f, 0.0f};

    Vector3 playerPos = player.pos();
    Vector3 offsetBack = Vector3Scale(forward, -6.0f);
    Vector3 offsetUp = {0.0f, 6.0f, 0.0f};
    cam.position = Vector3Add(Vector3Add(playerPos, offsetBack), offsetUp);
    cam.target = Vector3Add(playerPos, Vector3Scale(forward, 1.5f));
    cam.up = up;
    cam.fovy = 65.0f;
}

void SeismicSlamAttack::drawTweakHud(const Me &player)
{
    if (!tweakModeEnabled)
        return;

    const int baseX = 20;
    int y = 20;
    int line = 18;
    DrawText("Seismic Slam Tweak Mode", baseX, y, 20, ORANGE); y += line + 4;
    DrawText("F3: Toggle tweak mode", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("TAB: Switch to Dragon Claw tweak", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("Q/W/E/T: Select Point", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("Arrow keys: Side/Height", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("</>: Forward/Back control", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("R: Reset | F5: Save | F6: Load", baseX, y, 16, LIGHTGRAY); y += line;
    DrawText("Camera set to third-person view", baseX, y, 16, LIGHTGRAY);
}

void SeismicSlamAttack::resetArcDefaults()
{
    arcCurve = defaultArcCurve;
}

void SeismicSlamAttack::nudgeArc(const Vector3 &deltaP1, const Vector3 &deltaP2)
{
    arcCurve.p1 = Vector3Add(arcCurve.p1, deltaP1);
    arcCurve.p2 = Vector3Add(arcCurve.p2, deltaP2);
}

void SeismicSlamAttack::nudgeArcPoint(int pointIndex, const Vector3 &delta)
{
    switch (pointIndex)
    {
    case 0: arcCurve.p0 = Vector3Add(arcCurve.p0, delta); break;
    case 1: arcCurve.p1 = Vector3Add(arcCurve.p1, delta); break;
    case 2: arcCurve.p2 = Vector3Add(arcCurve.p2, delta); break;
    case 3: arcCurve.p3 = Vector3Add(arcCurve.p3, delta); break;
    }
}

bool SeismicSlamAttack::saveArcCurve() const
{
    std::ofstream out(arcSaveFilename);
    if (!out.is_open())
        return false;

    out << arcCurve.p0.x << ' ' << arcCurve.p0.y << ' ' << arcCurve.p0.z << ' '
        << arcCurve.p1.x << ' ' << arcCurve.p1.y << ' ' << arcCurve.p1.z << ' '
        << arcCurve.p2.x << ' ' << arcCurve.p2.y << ' ' << arcCurve.p2.z << ' '
        << arcCurve.p3.x << ' ' << arcCurve.p3.y << ' ' << arcCurve.p3.z << '\n';
    return true;
}

bool SeismicSlamAttack::loadArcCurve()
{
    std::ifstream in(arcSaveFilename);
    if (!in.is_open())
        return false;

    ArcCurve loaded = defaultArcCurve;
    if (!(in >> loaded.p0.x >> loaded.p0.y >> loaded.p0.z
            >> loaded.p1.x >> loaded.p1.y >> loaded.p1.z
            >> loaded.p2.x >> loaded.p2.y >> loaded.p2.z
            >> loaded.p3.x >> loaded.p3.y >> loaded.p3.z))
    {
        return false;
    }
    arcCurve = loaded;
    if (tweakModeEnabled)
    {
        refreshDebugArc(lastForward, lastRight, lastBasePos);
    }
    return true;
}
