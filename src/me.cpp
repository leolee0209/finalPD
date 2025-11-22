#include "me.hpp"
#include "scene.hpp"
#include "constant.hpp"
#include "raymath.h"

// Updates the player's state based on user input and physics
void Me::UpdateBody(Scene& scene, char side, char forward, bool jumpPressed, bool crouchHold)
{
    // Get player look direction
    float rot = this->camera.lookRotation.x;

    // Normalize input vector to prevent faster diagonal movement
    Vector2 input = {(float)side, (float)-forward};
    if ((side != 0) && (forward != 0))
        input = Vector2Normalize(input);

    float delta = GetFrameTime();

    // Apply gravity if the player is not on the ground
    if (!this->grounded)
        this->velocity.y -= GRAVITY * delta;

    // Handle jumping
    if (this->grounded && jumpPressed)
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
    float maxSpeed = (crouchHold ? CROUCH_SPEED : MAX_SPEED);
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
    for (int i = 0; i < 5; i++)
    {
        bool collided = false;
        for (auto& obj : scene.getObjects())
        {
            obj.UpdateOBB();
            CollisionResult result = this->o.collided(obj);
            if (result.collided)
            {
                collided = true;
                // Move the player out of the collided object
                this->position = Vector3Add(this->position, Vector3Scale(result.normal, result.penetration));
                this->o.pos = this->position;
                this->o.UpdateOBB();

                // Project velocity onto the normal and remove it to prevent sinking into the object
                float dot = Vector3DotProduct(this->velocity, result.normal);
                if (dot < 0) {
                    this->velocity = Vector3Subtract(this->velocity, Vector3Scale(result.normal, dot));
                }
            }
        }
        // If no collisions in an iteration, exit the loop
        if (!collided)
        {
            break;
        }
    }


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

// Updates the camera's position and orientation based on player movement
void Me::UpdateCamera(char side, char forward, bool crouchHold)
{
    this->camera.UpdateCamera(side, forward, crouchHold, this->position, this->grounded);
}

// Updates the projectile's state based on physics
void Projectile::UpdateBody(Scene* scene)
{
    const float delta = GetFrameTime();

    // Apply gravity if the projectile is in the air
    if (!this->grounded)
        this->velocity.y -= GRAVITY * delta;
    
    // Apply friction or air drag
    float decel = (this->grounded ? this->friction : this->airDrag);
    Vector3 hvel = {this->velocity.x * decel, 0.0f, this->velocity.z * decel};

    // If horizontal velocity is very low, set it to zero
    float hvelLength = Vector3Length(hvel); // Magnitude
    if (hvelLength < (MAX_SPEED * 0.02f))
        hvel = {0};
    this->velocity.x = hvel.x;
    this->velocity.z = hvel.z;

    // Update projectile position based on velocity
    this->position.x += this->velocity.x * delta;
    this->position.y += this->velocity.y * delta;
    this->position.z += this->velocity.z * delta;

    // Floor collision
    if (this->position.y <= 0.0f)
    {
        this->position.y = 0.0f;
        this->velocity.y = 0;
        this->grounded = true;
    }
    // Final update of the object's position
    this->o.pos = this->position;
}
