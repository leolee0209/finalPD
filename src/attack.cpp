#include "attack.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <raymath.h>
#include <iostream>
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
        updateTriplet(uc);
        break;
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
    float yaw;
    float pitch;

    if (uc.player == this->spawnedBy)
    {
        yaw = uc.player->getLookRotation().x;
        pitch = uc.player->getLookRotation().y;
    }
    else if (Enemy *enemy = dynamic_cast<Enemy *>(this->spawnedBy))
    {
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