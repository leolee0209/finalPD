#include "world.hpp"
#include <cmath>
#include <vector>
#include <iostream>
#include "me.hpp"
#include "model_constants.hpp"

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
    // Map definition from PLAN1.md
    const std::vector<std::string> map = {
        "#########################################",
        "# X X X X . . . . . . . . . . . . . . . #",
        "# X . . . . . . . . . . . . . . . . . . #",
        "# . . H . # # # # # # # # # # # # # # # #",
        "# . M s . # . . . . . . . . . . . . . . #",
        "# . . . . . . . M . . . . M . . . . . . #",
        "# . . . . # . . . . . . . . . . . . . . #",
        "# # # # # # # # . # # # # # # # . # # # #",
        "# . . . . # . . . . . H . . . # . . . . #",
        "# . . . . # . . . . . . . . . # . . . . #",
        "# . M . . . . . . ======= . . . . . M . #",
        "# . . . . # . . . . . . . . . # . . . . #",
        "# . . . . # . . . . . H . . . # . . . . #",
        "# # # . # # # # # # # . # # # # # . # # #",
        "# . . . . . . . . . # . # . . . . . . . #",
        "# . . . . . . . . . # . # . . . M . . . #",
        "# . . S . . . . . . . . . . . . . . . . #",
        "# . . . . . . . . . # . # . . . . . . . #",
        "#########################################"
    };

    // Tweakable variables
    float mapCellSize = 4.0f; // Size of one ASCII block in world units
    
    // Use natural tile size (2.0 width)
    // We want to fill the 4.0 cell with tiles.
    // 4.0 / 2.0 = 2 tiles wide.
    float tileWidth = TILE_MODEL_SIZE.x;
    float tileHeight = TILE_MODEL_SIZE.y;
    float tileDepth = TILE_MODEL_SIZE.z;
    
    int tilesPerCellX = (int)round(mapCellSize / tileWidth);
    int tilesPerCellZ = (int)round(mapCellSize / tileDepth); // Or maybe just 1 row deep?
    // Actually, walls are usually thin. Let's just place 2 tiles side-by-side to fill width, and 1 deep.
    // Or maybe 2x2?
    // If we want "tightly fit", let's fill the cell area.
    // But usually walls are just barriers.
    // Let's assume '#' is a solid block.
    
    float wallTileScale = 1.0f; // Use natural size
    float enemyScale = 1.32f; // 2.64 / 2.0 = 1.32 to match requested size
    
    // Center the map around startOffset
    float mapWidth = map[0].length() * mapCellSize;
    float mapHeight = map.size() * mapCellSize;
    Vector3 origin = {
        startOffset.x - mapWidth / 2.0f,
        startOffset.y,
        startOffset.z - mapHeight / 2.0f
    };

    // Add Floor
    Object* floor = new Object();
    float floorThickness = 1.0f;
    floor->size = {mapWidth * 1.5f, floorThickness, mapHeight * 1.5f};
    // Floor top at y=0. Floor center at y = -0.5
    floor->pos = {startOffset.x, -floorThickness / 2.0f, startOffset.z};
    floor->setAsBox(floor->size);
    floor->tint = WHITE; // White floor
    scene->AddStaticObject(floor);

    for (size_t z = 0; z < map.size(); ++z) {
        for (size_t x = 0; x < map[z].length(); ++x) {
            char cell = map[z][x];
            
            // Top-Left corner of the current map cell
            Vector3 cellOrigin = {
                origin.x + x * mapCellSize,
                0.0f,
                origin.z + z * mapCellSize
            };
            
            // Center of the current map cell (for single objects)
            Vector3 cellCenter = {
                cellOrigin.x + mapCellSize / 2.0f,
                0.0f,
                cellOrigin.z + mapCellSize / 2.0f
            };

            if (cell == '#') {
                // High Wall (WallTile)
                // Use Big Cube for collision
                // Visuals: Instanced tiles
                
                float wallScale = 1.5f;
                // Rotated dimensions:
                // Width (X) = Model.x * scale
                // Height (Y) = Model.z * scale (because rotated -90 around X)
                // Depth (Z) = Model.y * scale
                
                float tW = TILE_MODEL_SIZE.x * wallScale;
                float tH = TILE_MODEL_SIZE.z * wallScale;
                float tD = TILE_MODEL_SIZE.y * wallScale;
                
                int gridX = 2; // 2 * 3.0 = 6.0 > 4.0. Overlap to fill.
                int gridZ = 1; // 1 * 3.9 = 3.9 ~ 4.0. Good fit.
                int stackHeight = 4; // 4 * 2.4 = 9.6 high.
                
                // Add Big Collision Cube
                Object* collider = new Object();
                collider->size = {mapCellSize, stackHeight * tH, mapCellSize};
                collider->pos = {cellCenter.x, (stackHeight * tH) / 2.0f, cellCenter.z};
                collider->setAsBox(collider->size);
                collider->visible = false; // Invisible collision
                scene->AddStaticObject(collider);
                
                // Add Visual Instances
                Quaternion rot = QuaternionFromAxisAngle({1.0f, 0.0f, 0.0f}, -90.0f * DEG2RAD);
                
                int instanceCount = 0;
                for (int gy = 0; gy < stackHeight; gy++) {
                    for (int gx = 0; gx < gridX; gx++) {
                        for (int gz = 0; gz < gridZ; gz++) {
                            Vector3 pos;
                            // Distribute gridX tiles across mapCellSize
                            float stepX = mapCellSize / gridX;
                            pos.x = cellOrigin.x + (gx * stepX) + stepX / 2.0f;
                            
                            pos.y = (gy * tH) + tH / 2.0f;
                            
                            // Center Z
                            pos.z = cellCenter.z;
                            
                            scene->AddWallInstance(TileType::BAMBOO_1, pos, rot, {wallScale, wallScale, wallScale});
                            instanceCount++;
                        }
                    }
                }
                TraceLog(LOG_INFO, "Generated %d wall instances for cell at %f, %f", instanceCount, cellOrigin.x, cellOrigin.z);
            } else if (cell == '=') {
                // Low Cover (WallTile) - 1 high
                int gridX = 2;
                int gridZ = 2;
                for (int gx = 0; gx < gridX; gx++) {
                    for (int gz = 0; gz < gridZ; gz++) {
                        Vector3 pos;
                        pos.x = cellOrigin.x + (gx * tileWidth) + tileWidth / 2.0f;
                        pos.y = tileHeight / 2.0f;
                        float zGap = (mapCellSize - (gridZ * tileDepth)) / 2.0f;
                        pos.z = cellOrigin.z + zGap + (gz * tileDepth) + tileDepth / 2.0f;
                        
                        scene->AddWallTile(TileType::BAMBOO_1, pos, QuaternionIdentity(), wallTileScale);
                    }
                }
            } else if (cell == 'S') {
                // Start Point
                Vector3 spawnPos = cellCenter;
                spawnPos.y = 2.0f; // Slightly above ground
                scene->SetPlayerSpawnPosition(spawnPos);
            } else if (cell == 'M') {
                // Minion Spawner
                MinionEnemy* enemy = new MinionEnemy();
                Vector3 enemyPos = cellCenter;
                enemyPos.y = 1.0f; // On ground
                enemy->setPosition(enemyPos);
                enemy->setVisualScale(enemyScale);
                enemy->setTint(GREEN); // Debug color
                scene->em.addEnemy(enemy);
            } else if (cell == 'H') {
                // Shooter Perch (Sniper on high wall)
                // Create wall first (same as '#')
                int gridX = 2;
                int gridZ = 2;
                int stackHeight = 3;
                
                for (int gy = 0; gy < stackHeight; gy++) {
                    for (int gx = 0; gx < gridX; gx++) {
                        for (int gz = 0; gz < gridZ; gz++) {
                            Vector3 pos;
                            pos.x = cellOrigin.x + (gx * tileWidth) + tileWidth / 2.0f;
                            pos.y = (gy * tileHeight) + tileHeight / 2.0f;
                            float zGap = (mapCellSize - (gridZ * tileDepth)) / 2.0f;
                            pos.z = cellOrigin.z + zGap + (gz * tileDepth) + tileDepth / 2.0f;
                            
                            scene->AddWallTile(TileType::BAMBOO_1, pos, QuaternionIdentity(), wallTileScale);
                        }
                    }
                }

                // Place enemy on top
                ShooterEnemy* enemy = new ShooterEnemy();
                Vector3 enemyPos = cellCenter;
                enemyPos.y = (stackHeight * tileHeight) + 1.0f; // On top of wall
                enemy->setPosition(enemyPos);
                enemy->setVisualScale(enemyScale);
                enemy->setTint(PURPLE); // Debug color
                scene->em.addEnemy(enemy);
            } else if (cell == 's') {
                // The Support (Mini-Boss Arena)
                SupportEnemy* enemy = new SupportEnemy();
                Vector3 enemyPos = cellCenter;
                enemyPos.y = 1.0f;
                enemy->setPosition(enemyPos);
                enemy->setVisualScale(enemyScale);
                enemy->setTint(YELLOW); // Debug color
                scene->em.addEnemy(enemy);
            } else if (cell == 'X') {
                // The Exit
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
