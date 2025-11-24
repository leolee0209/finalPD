#include "me.hpp"
#include "scene.hpp"
#include "constant.hpp"
#include "raymath.h"
#include <iostream>

// Updates the player's state based on user input and physics
void Me::applyPlayerMovement(UpdateContext &uc)
{
    // Get player look direction
    float rot = this->camera.lookRotation.x;

    // Normalize input vector to prevent faster diagonal movement
    Vector2 input = {(float)uc.playerInput.side, (float)-uc.playerInput.forward};
    if ((uc.playerInput.side != 0) && (uc.playerInput.forward != 0))
        input = Vector2Normalize(input);

    float delta = GetFrameTime();

    // Apply gravity if the player is not on the ground
    if (!this->grounded)
        this->velocity.y -= GRAVITY * delta;

    // Handle jumping
    if (this->grounded && uc.playerInput.jumpPressed)
    {
        this->velocity.y = JUMP_FORCE;
        this->grounded = false;

        // Sound can be played at this moment
        // SetSoundPitch(fxJump, 1.0f + (GetRandomValue(-100, 100)*0.001));
        // PlaySound(fxJump);
    }

    // Calculate the direction the player is facing
    Vector3 front = {sinf(rot), 0.f, cosf(rot)};
    Vector3 right = {cosf(-rot), 0.f, sinf(-rot)};

    // Calculate desired movement direction based on input
    Vector3 desiredDir = {
        input.x * right.x + input.y * front.x,
        0.0f,
        input.x * right.z + input.y * front.z,
    };
    // Smoothly interpolate to the desired direction
    this->direction = Vector3Lerp(this->direction, desiredDir, CONTROL * delta);

    // Apply friction or air drag to horizontal velocity
    float decel = (this->grounded ? FRICTION : AIR_DRAG);
    Vector3 hvel = {this->velocity.x * decel, 0.0f, this->velocity.z * decel};

    // If horizontal velocity is very low, set it to zero
    float hvelLength = Vector3Length(hvel); // Magnitude
    if (hvelLength < (MAX_SPEED * 0.01f))
        hvel = {0};

    // This is what creates strafing
    float speed = Vector3DotProduct(hvel, this->direction);

    // Clamp acceleration to avoid exceeding max speed
    // More info here: https://youtu.be/v3zT3Z5apaM?t=165
    float maxSpeed = (uc.playerInput.crouchHold ? CROUCH_SPEED : MAX_SPEED);
    float accel = Clamp(maxSpeed - speed, 0.f, MAX_ACCEL * delta);

    // Apply acceleration to horizontal velocity
    hvel.x += this->direction.x * accel;
    hvel.z += this->direction.z * accel;

    this->velocity.x = hvel.x;
    this->velocity.z = hvel.z;

    // Update player position based on velocity
    this->position.x += this->velocity.x * delta;
    this->position.y += this->velocity.y * delta;
    this->position.z += this->velocity.z * delta;

    // Update the underlying object's position and OBB
    this->o.pos = this->position;
    this->o.UpdateOBB();

    // Iterative collision resolution to handle complex collisions and sliding
    Entity::resolveCollision(this, uc);

    // Floor collision
    if (this->position.y <= 0.0f)
    {
        this->position.y = 0.0f;
        this->velocity.y = 0.0f;
        this->grounded = true; // Enable jumping
    }
    // Final update of the object's position
    this->o.pos = this->position;
}

void Me::UpdateBody(UpdateContext &uc)
{
    this->applyPlayerMovement(uc);
    this->UpdateCamera(uc);
}

// Updates the camera's position and orientation based on player movement
void Me::UpdateCamera(UpdateContext &uc)
{
    this->camera.UpdateCamera(uc.playerInput.side, uc.playerInput.forward, uc.playerInput.crouchHold, this->position, this->grounded);
}

// Updates the projectile's body (movement, gravity, etc.)
void Projectile::UpdateBody(UpdateContext &uc)
{
    float delta = GetFrameTime();
    // 1. Apply gravity if not grounded
    if (!this->grounded)
    {
        this->velocity.y -= GRAVITY * delta;
    }

    // 2. Apply friction or air drag to horizontal velocity
    float decel = (this->grounded ? friction : airDrag);
    Vector3 hvel = {this->velocity.x * decel, 0.0f, this->velocity.z * decel};

    // 3. If horizontal velocity is very low, set it to zero
    if (Vector3Length(hvel) < (MAX_SPEED * 0.01f))
    {
        hvel = {0};
    }

    this->velocity.x = hvel.x;
    this->velocity.z = hvel.z;

    // 4. Update position based on velocity
    this->position.x += this->velocity.x * delta;
    this->position.y += this->velocity.y * delta;
    this->position.z += this->velocity.z * delta;

    // 5. Update the internal Object's position and OBB (needed before collision checks)
    this->o.pos = this->position;
    this->o.UpdateOBB();

    // do collision
    auto results = Object::collided(this->o, uc.scene);
    for (auto &result : results)
    {
        if (Enemy *e = dynamic_cast<Enemy *>(result.with))
        {
            auto dResult = DamageResult(10, result);
            uc.scene->em.damage(e, dResult);
        }
    }

    // 6. Floor collision
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

    // 7. Final Update OBB after all position changes
    this->o.UpdateOBB();
}

void Entity::resolveCollision(Entity *e, UpdateContext &uc)
{
    for (int i = 0; i < 5; i++)
    {
        std::vector<CollisionResult> results = Object::collided(e->o, uc.scene);
        if (e != uc.player)
        {
            auto playerResult = Object::collided(e->o, uc.player->obj());
            if (playerResult.collided)
                results.push_back(playerResult);
        }
        if (results.size() == 0)
            break;
        for (auto &result : results)
        {
            if (result.with == e)
                continue;
            e->position = Vector3Add(e->position, Vector3Scale(result.normal, result.penetration));
            e->o.pos = e->position;
            e->o.UpdateOBB();

            float dot = Vector3DotProduct(e->velocity, result.normal);
            if (dot < 0)
            {
                e->velocity = Vector3Subtract(e->velocity, Vector3Scale(result.normal, dot));
            }
            // if (fabs(result.normal.y) > 0.7f && result.normal.y > 0)
            // {
            //     e->grounded = true;
            // }
        }
    }
}
