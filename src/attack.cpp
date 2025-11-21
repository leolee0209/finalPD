#include "attack.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <raymath.h>
#include <iostream>
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
        cosf(pitch) * cosf(yaw) // Z component
    };

    // Normalize the forward vector
    forward = Vector3Normalize(forward);

    // Set the projectile's initial velocity to match the forward direction
    Vector3 vel = {-forward.x * 30 + this->spawnedBy->vel().x, 16, -forward.z * 30 + this->spawnedBy->vel().z};

    // Set the projectile's initial position slightly above the spawning entity
    Vector3 pos{this->spawnedBy->pos().x, this->spawnedBy->pos().y + 1.8f, this->spawnedBy->pos().z};
    Object o({1, 1, 1}, pos);
    o.setRotationFromForward(forward);

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

// TripletAttack implementation

void TripletAttack::spawnProjectile()
{
    if (projectiles.size() >= 3)
    {
        return;
    }

    state = SHOOTING;

    float yaw;
    float pitch;

    if (Me *player = dynamic_cast<Me *>(this->spawnedBy))
    {
        yaw = player->getLookRotation().x;
        pitch = player->getLookRotation().y;
    }
    else
    {
        // For now, do nothing if not player
        return;
    }

    Vector3 forward = {
        cosf(pitch) * sinf(yaw),
        0.0f,
        cosf(pitch) * cosf(yaw)};
    forward = Vector3Normalize(forward);

    // Set the projectile's initial velocity to match the forward direction
    Vector3 vel = {-forward.x * 30 + this->spawnedBy->vel().x, 16, -forward.z * 30 + this->spawnedBy->vel().z};

    // Set the projectile's initial position slightly above the spawning entity
    Vector3 pos{this->spawnedBy->pos().x, this->spawnedBy->pos().y + 1.8f, this->spawnedBy->pos().z};
    Object o({1, 1, 1}, pos);
    o.setRotationFromForward(forward);

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
}

void TripletAttack::update()
{
    // Update projectile physics first
    for (auto &p : projectiles)
    {
        p.UpdateBody();
    }

    switch (state)
    {
    case READY:
        // Do nothing, waiting for spawnProjectile to be called
        break;
    case SHOOTING:
        if (projectiles.size() >= 3)
        {
            // once all 3 projectiles are on the ground, switch to connecting
            bool allGrounded = true;
            for (const auto &p : projectiles)
            {
                if (!p.isGrounded())
                {
                    allGrounded = false;
                    break;
                }
            }
            if (allGrounded)
            {
                state = CONNECTING;
                connectingTimer = 0.0f;
                this->connectors.clear(); // ensure fresh connectors
            }
        }
        break;
    case CONNECTING:
    { // Increase timer and spawn connectors one-by-one.
        // Connector 0: between projectile[0] and projectile[1]
        // Connector 1: between projectile[1] and projectile[2]
        connectingTimer += GetFrameTime();

        auto spawnConnector = [&](int startS, int endS) -> void
        {
            Vector3 start = projectiles[startS].pos();
            Vector3 end = projectiles[endS].pos();
            Vector3 delta = Vector3Subtract(end, start);
            float length = Vector3Length(delta);
            Vector3 mid = Vector3Scale(Vector3Add(start, end), 0.5f);
            Vector3 forward = Vector3Normalize(delta);
            Object conn({connectorThickness, connectorThickness, length}, mid);
            conn.setRotationFromForward(forward);
            this->connectors.push_back(conn);
        };

        // spawn first connector immediately when entering CONNECTING (timer > 0)
        if (this->connectors.size() < 1 && connectingTimer > 0.0f)
        {
            if (projectiles.size() >= 2)
                spawnConnector(0, 1);
        }
        // spawn second connector at 0.5s (half-way)
        else if (this->connectors.size() < 2 && connectingTimer > 0.5f)
        {
            if (projectiles.size() >= 3)
                spawnConnector(1, 2);
        }
        else if (this->connectors.size() < 3 && connectingTimer > 1.0f)
        {
            if (projectiles.size() >= 3)
                spawnConnector(2, 0);
        }

        // After showing connectors for a short duration, advance to DONE
        if (connectingTimer > 2.0f) // keep connectors visible for 2 seconds
        {
            state = FINAL;
        }
    }
    break;
    case FINAL:
        if (!this->isFalling)
        {
            bool allConnectorsMaxHeight = true;
            for (auto &c : this->connectors)
            {
                if (c.getSize().y < TripletAttack::finalHeight)
                {
                    c.size.y += TripletAttack::finalGrowthRate;
                    allConnectorsMaxHeight = false;
                }
            }

            if (allConnectorsMaxHeight)
            {
                this->isFalling = true;
                float x = 0, y = 0, z = 0;
                float size = this->projectiles.size();
                for (auto &p : this->projectiles)
                {
                    x += p.pos().x;
                    y += p.pos().y;
                    z += p.pos().z;
                }
                this->averageConnectorPos = {x / size, y / size, z / size};
            }
        }
        else
        {
            bool allConnectorsAtDestination = true;
            const float fallingSpeed = 30.0f;
            for (auto &c : this->connectors)
            {
                if (Vector3DistanceSqr(c.pos, this->averageConnectorPos) > 0.1f)
                {
                    allConnectorsAtDestination = false;
                    Vector3 direction = Vector3Normalize(Vector3Subtract(this->averageConnectorPos, c.pos));
                    c.setRotationFromForward(direction);
                    c.pos = Vector3MoveTowards(c.pos, this->averageConnectorPos, fallingSpeed * GetFrameTime());
                }
            }

            if (allConnectorsAtDestination)
            {
                this->state = DONE;
            }
        }
        break;
    case DONE:
        // Clear everything and reset state
        projectiles.clear();
        connectors.clear();
        state = READY;
        break;
    }
}
