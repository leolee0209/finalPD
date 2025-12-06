#include "world.hpp"
#include <cmath>
#include <vector>
#include <iostream>

// Helper to get random float in range [min, max]
static float RandomFloat(float min, float max) {
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

// Helper to get random TileType
static TileType RandomTileType() {
    return (TileType)(rand() % (int)TileType::TILE_COUNT);
}

void WorldGenerator::Generate(Scene* scene) {
    // Generate biomes sequentially in Z
    // Maze -> Wall -> Arena
    
    // 1. Maze at origin (0, 0, 0)
    GenerateDiscardMaze(scene, {0, 0, 0});
    
    // 2. Great Wall starts after maze (approx Z=80)
    GenerateGreatWall(scene, {0, 0, 80});
    
    // 3. Arena starts after wall (approx Z=180, Y=30)
    GenerateArena(scene, {0, 30, 200});
}

void WorldGenerator::GenerateDiscardMaze(Scene* scene, Vector3 startOffset) {
    // Maze dimensions
    const int mazeWidth = 20;
    const int mazeLength = 20;
    const float tileScale = 1.0f;
    const float tileSize = 3.0f * tileScale; // Approx length of tile
    const float spacing = 4.0f; 

    // Simple grid generation with some randomness for "piles"
    for (int x = -mazeWidth / 2; x < mazeWidth / 2; x++) {
        for (int z = 0; z < mazeLength; z++) {
            
            // Skip center path to ensure walkability
            if (abs(x) < 2) continue;

            float noise = RandomFloat(0.0f, 1.0f);
            
            // 70% chance to place a tile pile
            if (noise > 0.3f) {
                Vector3 pos = Vector3Add(startOffset, {x * spacing, 0, z * spacing});
                
                // Random stack height 1 to 3
                int stackHeight = (int)RandomFloat(1, 4);
                
                for (int h = 0; h < stackHeight; h++) {
                    Vector3 tilePos = pos;
                    tilePos.y += h * 2.0f; // Stack vertically
                    
                    // Add some random rotation for "messy" look
                    float yRot = RandomFloat(0, 360) * DEG2RAD;
                    float xRot = RandomFloat(-10, 10) * DEG2RAD;
                    float zRot = RandomFloat(-10, 10) * DEG2RAD;
                    
                    Quaternion rot = QuaternionFromEuler(xRot, yRot, zRot);
                    
                    scene->AddTileObject(RandomTileType(), tilePos, rot, tileScale);
                }
            }
        }
    }
}

void WorldGenerator::GenerateGreatWall(Scene* scene, Vector3 startOffset) {
    // A series of steps rising up
    const int steps = 15;
    const float stepHeight = 2.0f;
    const float stepDepth = 4.0f;
    const float width = 30.0f;
    
    for (int i = 0; i < steps; i++) {
        float currentY = startOffset.y + i * stepHeight;
        float currentZ = startOffset.z + i * stepDepth;
        
        // Create a row of tiles for this step
        int tilesInRow = 10;
        for (int j = -tilesInRow / 2; j <= tilesInRow / 2; j++) {
            Vector3 pos = {j * 3.0f, currentY, currentZ};
            scene->AddTileObject(TileType::BACK, pos, QuaternionIdentity(), 1.0f);
        }
        
        // Add some "floating" platforms (Point Sticks) to the sides using simple Objects
        // We will use scene->AddStaticObject equivalent if available, or just use TileObject for now as placeholder
        // Using "ONE DOT" or similar simple tile as a platform on the side
        if (i % 3 == 0) {
             Vector3 leftPlat = {-width / 2 - 5.0f, currentY + 1.0f, currentZ};
             Vector3 rightPlat = {width / 2 + 5.0f, currentY + 1.0f, currentZ};
             
             Quaternion flatRot = QuaternionFromAxisAngle({1, 0, 0}, 90 * DEG2RAD);
             scene->AddTileObject(TileType::BAMBOO_1, leftPlat, flatRot, 1.5f);
             scene->AddTileObject(TileType::BAMBOO_1, rightPlat, flatRot, 1.5f);
        }
    }
}

void WorldGenerator::GenerateArena(Scene* scene, Vector3 startOffset) {
    // Large circular floor or arena boundary
    float radius = 30.0f;
    int segments = 32;
    
    // Floor (using flat tiles arranged in circle/grid)
    for (int x = -10; x <= 10; x++) {
        for (int z = -10; z <= 10; z++) {
            if (x*x + z*z < 100) { // Circle
                Vector3 pos = Vector3Add(startOffset, {x * 3.0f, 0, z * 3.0f});
                Quaternion flatRot = QuaternionFromAxisAngle({1, 0, 0}, 90 * DEG2RAD);
                 scene->AddTileObject(TileType::BACK, pos, flatRot, 1.0f);
            }
        }
    }
    
    // Wall around
    for (int i = 0; i < segments; i++) {
        float angle = (float)i / segments * 2 * PI;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;
        
        Vector3 pos = Vector3Add(startOffset, {x, 1.5f, z});
        // Rotate to face center
        Quaternion rot = QuaternionFromAxisAngle({0, 1, 0}, -angle * RAD2DEG * DEG2RAD); // Basic rotation
        
        // Stack 3 high
        for (int h = 0; h < 3; h++) {
            Vector3 stackPos = pos;
            stackPos.y += h * 2.0f;
            scene->AddTileObject(TileType::WIND_NORTH, stackPos, rot, 1.2f);
        }
    }
    
    // Center Tower
    Vector3 center = startOffset;
    center.y += 0.0f;
    for (int h = 0; h < 5; h++) {
        scene->AddTileObject(TileType::DRAGON_RED, {center.x, center.y + h * 3.0f, center.z}, QuaternionIdentity(), 1.5f);
    }
}
