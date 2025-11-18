#include "attack.hpp"
#include "scene.hpp"
#include "raymath.h"
#include <iostream>
#include <memory>
void Attack::play(Me &player, Scene &scene)
{
    // Calculate the forward direction based on the player's lookRotation
    const float yaw = player.getLookRotation().x;   // Horizontal rotation
    const float pitch = player.getLookRotation().y; // Vertical rotation

    // Compute the forward direction vector
    Vector3 forward = {
        cosf(pitch) * sinf(yaw), // X component
        0.0f,
        cosf(pitch) * cosf(yaw) // Z component
    };

    // Normalize the forward vector
    forward = Vector3Normalize(forward);

    // Set the projectile's initial velocity to match the forward direction
    Vector3 vel = {-forward.x * 30 + player.vel().x, 16, -forward.z * 30 + player.vel().z}; // Adjust speed as needed

    Vector3 pos{player.pos().x, player.pos().y + 1.8f, player.pos().z};
    Object o({1, 1, 1}, pos);

    // Create the projectile object
    Projectile projectile(
        pos,     // Start at the player's position
        vel,     // Set velocity to match the forward direction
        forward, // Direction
        false,   // Not grounded
        o        // Default object
    );

    // Add the projectile to the scene
    scene.addEntity(std::move(std::make_unique<Projectile>(projectile)));
    std::cout << vel.x << "," << vel.y << "," << vel.z << std::endl;
}
