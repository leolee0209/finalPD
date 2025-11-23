#include "me.hpp"       // Contains Enemy class definition
#include "scene.hpp"    // For Scene class and its methods
#include "raymath.h"    // For Vector3 operations
#include "constant.hpp" // For constants like GRAVITY, FRICTION, AIR_DRAG, MAX_SPEED, MAX_ACCEL
#include <iostream>
// Implement Enemy::UpdateBody
void Enemy::UpdateBody(UpdateContext& uc)
{
    float delta = GetFrameTime();

    // 1. Apply gravity if not grounded
    if (!this->grounded)
    {
        this->velocity.y -= GRAVITY * delta;
    }

    // 2. Calculate direction to player and normalize
    Vector3 directionToPlayer = Vector3Subtract(uc.player->pos(), this->position); // Use player.pos()
    directionToPlayer.y = 0;                                                   // Ignore Y-axis for horizontal movement direction
    if (Vector3LengthSqr(directionToPlayer) > 0.001f)
    { // Avoid normalization of zero vector
        directionToPlayer = Vector3Normalize(directionToPlayer);
    }
    else
    {
        directionToPlayer = {0}; // If player is at the same position, stop horizontal movement desire
    }

    // 3. Set enemy's desired movement direction
    this->direction = directionToPlayer; // This 'direction' is the desired movement input

    // 4. Apply friction or air drag to horizontal velocity
    float decel = (this->grounded ? FRICTION : AIR_DRAG);
    Vector3 hvel = {this->velocity.x * decel, 0.0f, this->velocity.z * decel};

    // 5. If horizontal velocity is very low, set it to zero
    float hvelLength = Vector3Length(hvel);
    if (hvelLength < (MAX_SPEED * 0.01f))
    {
        hvel = {0};
    }

    // This is similar to player strafing, but for enemy it's more about how it accelerates
    float speed = Vector3DotProduct(hvel, this->direction); // Speed in current desired direction

    // 6. Clamp acceleration to avoid exceeding max speed
    float maxEnemySpeed = 3.0f; // Adjust this for enemy's max speed, can be a constant
    float accel = Clamp(maxEnemySpeed - speed, 0.f, MAX_ACCEL * delta);

    // 7. Apply acceleration to horizontal velocity
    hvel.x += this->direction.x * accel;
    hvel.z += this->direction.z * accel;

    this->velocity.x = hvel.x;
    this->velocity.z = hvel.z;

    // 8. Update position based on velocity
    this->position.x += this->velocity.x * delta;
    this->position.y += this->velocity.y * delta;
    this->position.z += this->velocity.z * delta;

    // 9. Update the internal Object's position and OBB (needed before collision checks)
    this->o.pos = this->position;
    this->o.UpdateOBB();

    // 10. Iterative collision resolution with scene objects and player
    for (int i = 0; i < 5; i++)
    {
        bool collidedThisIteration = false;

        std::vector<CollisionResult> results = Object::collided(this->o,uc.scene);
        for (CollisionResult &result : results)
        {
            collidedThisIteration = true;
            // Move the enemy out of the collided object
            this->position = Vector3Add(this->position, Vector3Scale(result.normal, result.penetration));
            this->o.pos = this->position;
            this->o.UpdateOBB();

            float dot = Vector3DotProduct(this->velocity, result.normal);
            if (dot < 0)
            {
                this->velocity = Vector3Subtract(this->velocity, Vector3Scale(result.normal, dot));
            }
            if (fabs(result.normal.y) > 0.7f && result.normal.y > 0)
            {
                this->grounded = true;
            }
        }

        // Check collision with the player
        if (&uc.player->obj() != &this->o) // Ensure enemy doesn't collide with its own object
        {
            uc.player->obj().UpdateOBB(); // Ensure player's OBB is up-to-date
            CollisionResult result = Object::collided(this->o, uc.player->obj());
            if (result.collided)
            {
                collidedThisIteration = true;
                // Move the enemy out of the player
                this->position = Vector3Add(this->position, Vector3Scale(result.normal, result.penetration));
                this->o.pos = this->position;
                this->o.UpdateOBB();

                float dot = Vector3DotProduct(this->velocity, result.normal);
                if (dot < 0)
                {
                    this->velocity = Vector3Subtract(this->velocity, Vector3Scale(result.normal, dot));
                }
                if (fabs(result.normal.y) > 0.7f && result.normal.y > 0)
                {
                    this->grounded = true;
                }
            }
        }

        if (!collidedThisIteration)
        {
            break;
        }
    }

    // 11. Floor collision (final check, after general collision resolution)
    if (this->position.y <= 0.0f)
    {
        this->position.y = 0.0f;
        this->velocity.y = 0.0f;
        this->grounded = true;
    }
    else if (this->grounded && this->velocity.y < 0.01f)
    {
        this->grounded = false;
    }

    // 12. Make the enemy rotate to face the player (only horizontally)
    if (Vector3LengthSqr(directionToPlayer) > 0.001f)
    {
        this->o.setRotationFromForward(directionToPlayer);
    }

    // 13. Final Update OBB after all position changes
    this->o.UpdateOBB();
}