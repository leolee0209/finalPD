#include "attack.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <raymath.h>
void ThousandAttack::activateFinal()
{
    float x = 0, y = 0, z = 0;
    float size = this->projectiles.size();
    for (auto &p : this->projectiles)
    {
        x += p.pos().x;
        y += p.pos().y;
        z += p.pos().z;
    }
    this->dest = {x / size, y / size, z / size};
    for (auto &p : this->projectiles)
    {
        Vector3 newDir = Vector3Normalize({this->dest.x - p.pos().x, 0, this->dest.z - p.pos().z});
        Vector3 newVel = {newDir.x * ThousandAttack::finalVel, 0, newDir.z * ThousandAttack::finalVel};
        p = Projectile(p.pos(), newVel, newDir, p.isGrounded(), p.obj(), 1, 1);
    }
}
bool ThousandAttack::endFinal()
{
    std::vector<int> remove;
    for (int i = 0; i < this->projectiles.size(); i++)
    {
        const Projectile &p = this->projectiles[i];
        if (Vector3DistanceSqr(p.pos(), this->dest) <= 0.1f)
        {
            remove.push_back(i);
        }
        else
        {
            Vector3 toDest = Vector3Subtract(this->dest, p.pos());
            float dotProduct = Vector3DotProduct(toDest, p.vel());
            if (dotProduct <= 0) // Negative dot product means the projectile is moving away
            {
                remove.push_back(i);
            }
        }
    }
    for (const auto &i : remove)
    {
        this->projectiles.erase(this->projectiles.begin() + i);
        this->count--;
    }

    return this->projectiles.size() == 0;
}
void ThousandAttack::spawnProjectile()
{
    float yaw;
    float pitch;
    if (Me *player = dynamic_cast<Me *>(this->spawnedBy)) // is Me(player)
    {                                                     // Calculate the forward direction based on the player's lookRotation
        yaw = player->getLookRotation().x;                // Horizontal rotation
        pitch = player->getLookRotation().y;              // Vertical rotation
    }
    else // is entity TODO
    {
    }
    // Compute the forward direction vector
    Vector3 forward = {
        cosf(pitch) * sinf(yaw), // X component
        0.0f,
        cosf(pitch) * cosf(yaw) // Z component
    };

    // Normalize the forward vector
    forward = Vector3Normalize(forward);

    // Set the projectile's initial velocity to match the forward direction
    Vector3 vel = {-forward.x * 30 + this->spawnedBy->vel().x, 16, -forward.z * 30 + this->spawnedBy->vel().z}; // Adjust speed as needed

    Vector3 pos{this->spawnedBy->pos().x, this->spawnedBy->pos().y + 1.8f, this->spawnedBy->pos().z};
    Object o({1, 1, 1}, pos);

    // Create the projectile object
    Projectile projectile(
        pos,     // Start at the spawnedBy's position
        vel,     // Set velocity to match the forward direction
        forward, // Direction
        false,   // Not grounded
        o,       // Default object
        FRICTION,
        AIR_DRAG);

    this->projectiles.push_back(projectile);
    this->count++;
}

void ThousandAttack::update()
{
    for (auto &p : this->projectiles)
    {
        p.UpdateBody();
    }
    if (!this->activated && this->projectiles.size() >= ThousandAttack::activate)
    {
        if (this->projectiles.back().isGrounded())
        {
            this->activated = true;
            activateFinal();
        }
    }
    if (this->activated && endFinal())
    {
        this->projectiles.clear();
        this->activated = false;
        this->count = 0;
        this->dest = {0};
    }
}
