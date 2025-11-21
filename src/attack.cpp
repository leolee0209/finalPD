#include "attack.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <raymath.h>

// Activates the final phase of the ThousandAttack
// All projectiles move toward the calculated destination point
void ThousandAttack::activateFinal()
{
    float x = 0, y = 0, z = 0;
    float size = this->projectiles.size();

    // Calculate the average position of all projectiles to determine the destination
    for (auto &p : this->projectiles)
    {
        x += p.pos().x;
        y += p.pos().y;
        z += p.pos().z;
    }
    this->dest = {x / size, y / size, z / size}; // Average position as the destination

    // Update each projectile to move toward the destination
    for (auto &p : this->projectiles)
    {
        Vector3 newDir = Vector3Normalize({this->dest.x - p.pos().x, 0, this->dest.z - p.pos().z});
        Vector3 newVel = {newDir.x * ThousandAttack::finalVel, 0, newDir.z * ThousandAttack::finalVel};
        p = Projectile(p.pos(), newVel, newDir, p.isGrounded(), p.obj(), 1, 1);
    }
}

// Checks if the final phase of the attack is complete
// Removes projectiles that have reached the destination or overshot it
bool ThousandAttack::endFinal()
{
    std::vector<int> remove; // Indices of projectiles to remove

    for (int i = 0; i < this->projectiles.size(); i++)
    {
        const Projectile &p = this->projectiles[i];

        // Check if the projectile is within a small margin of the destination
        if (Vector3DistanceSqr(p.pos(), this->dest) <= 0.1f)
        {
            remove.push_back(i);
        }
        else
        {
            // Check if the projectile has overshot the destination
            Vector3 toDest = Vector3Subtract(this->dest, p.pos());
            float dotProduct = Vector3DotProduct(toDest, p.vel());
            if (dotProduct <= 0) // Negative dot product means it's moving away
            {
                remove.push_back(i);
            }
        }
    }

    // Remove projectiles that are no longer valid
    for (const auto &i : remove)
    {
        this->projectiles[i] = this->projectiles.back();
        this->projectiles.pop_back();
        this->count--;
    }

    // Return true if all projectiles have been removed
    return this->projectiles.size() == 0;
}

// Spawns a new projectile for the ThousandAttack
void ThousandAttack::spawnProjectile()
{
    float yaw;
    float pitch;

    // Determine the spawning entity (player or enemy)
    if (Me *player = dynamic_cast<Me *>(this->spawnedBy)) // If spawned by the player
    {
        yaw = player->getLookRotation().x;   // Horizontal rotation
        pitch = player->getLookRotation().y; // Vertical rotation
    }
    else // If spawned by another entity (e.g., enemy)
    {
        // TODO: Implement behavior for other entities
    }

    // Compute the forward direction vector based on yaw and pitch
    Vector3 forward = {
        cosf(pitch) * sinf(yaw), // X component
        0.0f,
        cosf(pitch) * cosf(yaw)  // Z component
    };

    // Normalize the forward vector
    forward = Vector3Normalize(forward);

    // Set the projectile's initial velocity to match the forward direction
    Vector3 vel = {-forward.x * 30 + this->spawnedBy->vel().x, 16, -forward.z * 30 + this->spawnedBy->vel().z};

    // Set the projectile's initial position slightly above the spawning entity
    Vector3 pos{this->spawnedBy->pos().x, this->spawnedBy->pos().y + 1.8f, this->spawnedBy->pos().z};
    Object o({1, 1, 1}, pos);

    // Create the projectile and add it to the list
    Projectile projectile(
        pos,     // Start position
        vel,     // Initial velocity
        forward, // Direction
        false,   // Not grounded
        o,       // Object representation
        FRICTION,
        AIR_DRAG);

    this->projectiles.push_back(projectile);
    this->count++;
}

// Updates the Entities controlled by ThousandAttack
// Handles projectile movement, activation of the final phase, and cleanup
void ThousandAttack::update()
{
    // Update all projectiles
    for (auto &p : this->projectiles)
    {
        p.UpdateBody();
    }

    // Check if the final phase should be activated
    if (!this->activated && this->projectiles.size() >= ThousandAttack::activate)
    {
        if (this->projectiles.back().isGrounded())
        {
            this->activated = true;
            activateFinal();
        }
    }

    // Check if the final phase is complete and reset the attack
    if (this->activated && endFinal())
    {
        this->projectiles.clear();
        this->activated = false;
        this->count = 0;
        this->dest = {0};
    }
}
