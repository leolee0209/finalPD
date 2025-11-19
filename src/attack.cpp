#include "attack.hpp"
#include <raylib.h>
#include <raymath.h>
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
        o        // Default object
    );

    this->projectiles.push_back(projectile);
    this->count++;
}

void ThousandAttack::update()
{
    for (auto &p : this->projectiles)
    {
        p.UpdateBody();
    }
}
