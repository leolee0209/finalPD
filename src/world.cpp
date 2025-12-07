#include "world.hpp"
#include <cmath>
#include <vector>
#include <iostream>
#include "me.hpp"
#include "model_constants.hpp"

// Helper to get random float in range [min, max]
static float RandomFloat(float min, float max)
{
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

// Helper to get random TileType
static TileType RandomTileType()
{
    return (TileType)(rand() % (int)TileType::TILE_COUNT);
}

// Helper to check if a sub-grid position is occupied
static bool IsOccupied(const std::vector<std::string> &map, int mx, int mz, int gx, int gz, int gridX, int gridZ)
{
    // Check if inside current cell
    if (gx >= 0 && gx < gridX && gz >= 0 && gz < gridZ)
    {
        return true; // Occupied by the stack itself or neighbor stack in same cell
    }

    // Outside current cell. Check neighbor cell.
    int checkMx = mx;
    int checkMz = mz;

    if (gx < 0)
        checkMx--;
    else if (gx >= gridX)
        checkMx++;

    if (gz < 0)
        checkMz--;
    else if (gz >= gridZ)
        checkMz++;

    // Check bounds
    if (checkMz < 0 || checkMz >= (int)map.size())
        return false;
    if (checkMx < 0 || checkMx >= (int)map[checkMz].size())
        return false;

    char cell = map[checkMz][checkMx];
    return (cell == '#' || cell == '=' || cell == 'H');
}

static void TryAddExtraTile(Scene *scene, Vector3 stackPos, const std::vector<std::string> &map, int mx, int mz, int gx, int gz, int gridX, int gridZ, float tileWidth, float tileDepth, int stackHeight)
{
    int chance = 5;
    if (stackHeight <= 1) chance = 1; // Less likely for short walls

    if (rand() % 100 >= chance)
        return; // 5% chance (or 1%)

    struct Dir
    {
        int dx, dz;
        float angle;
    };
    std::vector<Dir> candidates;

    // Check 4 neighbors
    if (!IsOccupied(map, mx, mz, gx + 1, gz, gridX, gridZ))
        candidates.push_back({1, 0, 0.0f}); // Right
    if (!IsOccupied(map, mx, mz, gx - 1, gz, gridX, gridZ))
        candidates.push_back({-1, 0, 180.0f}); // Left
    if (!IsOccupied(map, mx, mz, gx, gz + 1, gridX, gridZ))
        candidates.push_back({0, 1, 90.0f}); // Down
    if (!IsOccupied(map, mx, mz, gx, gz - 1, gridX, gridZ))
        candidates.push_back({0, -1, -90.0f}); // Up

    if (candidates.empty())
        return;

    Dir chosen = candidates[rand() % candidates.size()];

    bool leaning = (rand() % 2 == 0);

    Vector3 pos = stackPos;
    pos.x += chosen.dx * tileWidth;
    pos.z += chosen.dz * tileDepth;

    Quaternion rot;
    if (leaning)
    {
        // Leaning against the wall
        // Face direction away from the tile it leans against.
        // chosen.angle points away from wall.
        
        // Rotate around Y to face direction
        Quaternion qY = QuaternionFromAxisAngle({0, 1, 0}, (chosen.angle) * DEG2RAD);
        // Tilt up/back around X.
        Quaternion qX = QuaternionFromAxisAngle({1, 0, 0}, -30.0f * DEG2RAD); // More tilt

        // Apply tilt (local X) then facing (global Y) -> qY * qX
        rot = QuaternionMultiply(qY, qX);
        pos.y = 0.5f; // Adjust height
    }
    else
    {
        // Lying
        rot = QuaternionFromAxisAngle({0, 1, 0}, RandomFloat(0, 360) * DEG2RAD); // Random rotation on floor
        pos.y = 0.5f;
    }

    scene->AddWallInstance(TileType::BAMBOO_1, pos, rot, {1.0f, 1.0f, 1.0f});
    
    // Add collision object
    Object* obj = new Object();
    obj->pos = pos;
    obj->rotation = rot;
    // Use TILE_MODEL_SIZE for box dimensions (local space)
    // Assuming TILE_MODEL_SIZE is {width, thickness, height} or similar.
    // Since we use AddWallInstance with scale 1.0, we use base model size.
    obj->setAsBox(TILE_MODEL_SIZE); 
    obj->visible = false;
    obj->UpdateOBB();
    scene->AddStaticObject(obj);
}

void WorldGenerator::Generate(Scene *scene)
{
    // Generate biomes sequentially in Z
    // Maze -> Wall -> Arena

    // 1. Maze at origin (0, 0, 0)
    GenerateDiscardMaze(scene, {0, 0, 0});

    // 2. Great Wall starts after maze (approx Z=80)
    GenerateGreatWall(scene, {0, 0, 80});

    // 3. Arena starts after wall (approx Z=180, Y=30)
    GenerateArena(scene, {0, 30, 200});
}

void WorldGenerator::GenerateDiscardMaze(Scene *scene, Vector3 startOffset)
{
    // Map definition from PLAN1.md
    const std::vector<std::string> map = {
        "#########################################",
        "#                 =                     #",
        "#                 =                     #",
        "#     H           =                     #",
        "#   M s           =                     #",
        "#         #     M         M             #",
        "#         #                             #",
        "#############      ######################",
        "#         =           H       #         #",
        "#         =                             #",
        "#   M     ===============           M   #",
        "#                                       #",
        "#                     H                 #",
        "#####======     ############      #######",
        "#                       #               #",
        "#                       #       M       #",
        "#     S                                 #",
        "#                       #               #",
        "#########################################"};

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
    float enemyScale = 1.32f;   // 2.64 / 2.0 = 1.32 to match requested size

    // Center the map around startOffset
    float mapWidth = map[0].length() * mapCellSize;
    float mapHeight = map.size() * mapCellSize;
    Vector3 origin = {
        startOffset.x - mapWidth / 2.0f,
        startOffset.y,
        startOffset.z - mapHeight / 2.0f};

    // Add Floor
    Object *floor = new Object();
    float floorThickness = 1.0f;
    floor->size = {mapWidth * 1.5f, floorThickness, mapHeight * 1.5f};
    // Floor top at y=0. Floor center at y = -0.5
    floor->pos = {startOffset.x, -floorThickness / 2.0f, startOffset.z};
    floor->setAsBox(floor->size);
    floor->tint = WHITE; // White floor
    scene->AddStaticObject(floor);

    for (size_t z = 0; z < map.size(); ++z)
    {
        for (size_t x = 0; x < map[z].length(); ++x)
        {
            char cell = map[z][x];

            // Top-Left corner of the current map cell
            Vector3 cellOrigin = {
                origin.x + x * mapCellSize,
                0.0f,
                origin.z + z * mapCellSize};

            // Center of the current map cell (for single objects)
            Vector3 cellCenter = {
                cellOrigin.x + mapCellSize / 2.0f,
                0.0f,
                cellOrigin.z + mapCellSize / 2.0f};

            if (cell == '#')
            {
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

                int gridX = 2;       // 2 * 3.0 = 6.0 > 4.0. Overlap to fill.
                int gridZ = 1;       // 1 * 3.9 = 3.9 ~ 4.0. Good fit.
                int stackHeight = 4; // 4 * 2.4 = 9.6 high.

                // Add Big Collision Cube
                Object *collider = new Object();
                collider->size = {mapCellSize, stackHeight * tH, mapCellSize};
                collider->pos = {cellCenter.x, (stackHeight * tH) / 2.0f, cellCenter.z};
                collider->setAsBox(collider->size);
                collider->visible = false; // Invisible collision
                scene->AddStaticObject(collider);

                // Add Visual Instances
                Quaternion rot = QuaternionFromAxisAngle({1.0f, 0.0f, 0.0f}, -90.0f * DEG2RAD);

                int instanceCount = 0;
                for (int gy = 0; gy < stackHeight; gy++)
                {
                    for (int gx = 0; gx < gridX; gx++)
                    {
                        for (int gz = 0; gz < gridZ; gz++)
                        {
                            Vector3 pos;
                            // Distribute gridX tiles across mapCellSize
                            float stepX = mapCellSize / gridX;
                            pos.x = cellOrigin.x + (gx * stepX) + stepX / 2.0f;

                            pos.y = (gy * tH) + tH / 2.0f;

                            // Center Z
                            pos.z = cellCenter.z;

                            scene->AddWallInstance(TileType::BAMBOO_1, pos, rot, {wallScale, wallScale, wallScale});
                            instanceCount++;

                            if (gy == 0)
                            {
                                TryAddExtraTile(scene, pos, map, x, z, gx, gz, gridX, gridZ, tW, tD, stackHeight);
                            }
                        }
                    }
                }
                TraceLog(LOG_INFO, "Generated %d wall instances for cell at %f, %f", instanceCount, cellOrigin.x, cellOrigin.z);
            }
            else if (cell == '=')
            {
                // Low Cover (WallTile) - Variable height
                int gridX = 2;
                int gridZ = 2;

                float tW = TILE_MODEL_SIZE.x * wallTileScale;
                float tH = TILE_MODEL_SIZE.z * wallTileScale; // Height when standing (-90 rot)
                float tD = TILE_MODEL_SIZE.y * wallTileScale; // Depth when standing

                // Add collision object for the low wall block
                // We can approximate the whole cell as a box, or per stack.
                // Since height varies, let's do per stack or just one big box if we want simple.
                // But height varies randomly.
                // Let's add collision per stack or per tile?
                // Per tile is too many objects.
                // Let's add one collision object for the whole cell with max height? No, that blocks shooting over.
                // Let's add collision objects for each stack.

                Quaternion rot = QuaternionFromAxisAngle({1.0f, 0.0f, 0.0f}, -90.0f * DEG2RAD);

                for (int gx = 0; gx < gridX; gx++)
                {
                    for (int gz = 0; gz < gridZ; gz++)
                    {
                        // Determine height for this stack
                        int height = 1;
                        int r = rand() % 100;
                        if (r < 20)
                            height += 2; // 20% chance +2 (Total 3)
                        else if (r < 50)
                            height += 1; // 30% chance +1 (Total 2)

                        // Add collision for this stack
                        Object* collider = new Object();
                        float stackH = height * tH;
                        collider->size = {tW, stackH, tD};
                        
                        Vector3 pos;
                        pos.x = cellOrigin.x + (gx * tileWidth) + tileWidth / 2.0f;
                        pos.y = stackH / 2.0f;
                        float zGap = (mapCellSize - (gridZ * tileDepth)) / 2.0f;
                        pos.z = cellOrigin.z + zGap + (gz * tileDepth) + tileDepth / 2.0f;
                        
                        collider->pos = pos;
                        collider->setAsBox(collider->size);
                        collider->visible = false;
                        scene->AddStaticObject(collider);

                        for (int h = 0; h < height; h++)
                        {
                            Vector3 tilePos = pos;
                            tilePos.y = (h * tH) + tH / 2.0f;

                            // Use AddWallInstance instead of AddWallTile for consistency
                            scene->AddWallInstance(TileType::BAMBOO_1, tilePos, rot, {wallTileScale, wallTileScale, wallTileScale});

                            if (h == 0)
                            {
                                TryAddExtraTile(scene, tilePos, map, x, z, gx, gz, gridX, gridZ, tW, tD, height);
                            }
                        }
                    }
                }
            }
            else if (cell == 'S')
            {
                // Start Point
                Vector3 spawnPos = cellCenter;
                spawnPos.y = 2.0f; // Slightly above ground
                scene->SetPlayerSpawnPosition(spawnPos);
            }
            else if (cell == 'M')
            {
                // Minion Spawner
                SummonerEnemy *enemy = new SummonerEnemy();
                Vector3 enemyPos = cellCenter;
                enemyPos.y = 1.0f; // On ground
                enemy->setPosition(enemyPos);
                enemy->setVisualScale(enemyScale);
                enemy->setTint(GREEN); // Debug color
                scene->em.addEnemy(enemy);
            }
            else if (cell == 'H')
            {
                // Shooter Perch (Sniper on high wall)
                // Create wall first (same as '#')
                int gridX = 2;
                int gridZ = 2;
                int stackHeight = 3;

                for (int gy = 0; gy < stackHeight; gy++)
                {
                    for (int gx = 0; gx < gridX; gx++)
                    {
                        for (int gz = 0; gz < gridZ; gz++)
                        {
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
                ShooterEnemy *enemy = new ShooterEnemy();
                Vector3 enemyPos = cellCenter;
                enemyPos.y = (stackHeight * tileHeight) + 1.0f; // On top of wall
                enemy->setPosition(enemyPos);
                enemy->setVisualScale(enemyScale);
                enemy->setTint(PURPLE); // Debug color
                scene->em.addEnemy(enemy);
            }
            else if (cell == 's')
            {
                // The Support (Mini-Boss Arena)
                SupportEnemy *enemy = new SupportEnemy();
                Vector3 enemyPos = cellCenter;
                enemyPos.y = 1.0f;
                enemy->setPosition(enemyPos);
                enemy->setVisualScale(enemyScale);
                enemy->setTint(YELLOW); // Debug color
                scene->em.addEnemy(enemy);
            }
            else if (cell == 'X')
            {
                // The Exit
            }
        }
    }
}

void WorldGenerator::GenerateGreatWall(Scene *scene, Vector3 startOffset)
{
    // A series of steps rising up
    const int steps = 15;
    const float stepHeight = 2.0f;
    const float stepDepth = 4.0f;
    const float width = 30.0f;

    for (int i = 0; i < steps; i++)
    {
        float currentY = startOffset.y + i * stepHeight;
        float currentZ = startOffset.z + i * stepDepth;

        // Create a row of tiles for this step
        int tilesInRow = 10;
        for (int j = -tilesInRow / 2; j <= tilesInRow / 2; j++)
        {
            Vector3 pos = {j * 3.0f, currentY, currentZ};
            scene->AddTileObject(TileType::BACK, pos, QuaternionIdentity(), 1.0f);
        }

        // Add some "floating" platforms (Point Sticks) to the sides using simple Objects
        // We will use scene->AddStaticObject equivalent if available, or just use TileObject for now as placeholder
        // Using "ONE DOT" or similar simple tile as a platform on the side
        if (i % 3 == 0)
        {
            Vector3 leftPlat = {-width / 2 - 5.0f, currentY + 1.0f, currentZ};
            Vector3 rightPlat = {width / 2 + 5.0f, currentY + 1.0f, currentZ};

            Quaternion flatRot = QuaternionFromAxisAngle({1, 0, 0}, 90 * DEG2RAD);
            scene->AddTileObject(TileType::BAMBOO_1, leftPlat, flatRot, 1.5f);
            scene->AddTileObject(TileType::BAMBOO_1, rightPlat, flatRot, 1.5f);
        }
    }
}

void WorldGenerator::GenerateArena(Scene *scene, Vector3 startOffset)
{
    // Large circular floor or arena boundary
    float radius = 30.0f;
    int segments = 32;

    // Floor (using flat tiles arranged in circle/grid)
    for (int x = -10; x <= 10; x++)
    {
        for (int z = -10; z <= 10; z++)
        {
            if (x * x + z * z < 100)
            { // Circle
                Vector3 pos = Vector3Add(startOffset, {x * 3.0f, 0, z * 3.0f});
                Quaternion flatRot = QuaternionFromAxisAngle({1, 0, 0}, 90 * DEG2RAD);
                scene->AddTileObject(TileType::BACK, pos, flatRot, 1.0f);
            }
        }
    }

    // Wall around
    for (int i = 0; i < segments; i++)
    {
        float angle = (float)i / segments * 2 * PI;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        Vector3 pos = Vector3Add(startOffset, {x, 1.5f, z});
        // Rotate to face center
        Quaternion rot = QuaternionFromAxisAngle({0, 1, 0}, -angle * RAD2DEG * DEG2RAD); // Basic rotation

        // Stack 3 high
        for (int h = 0; h < 3; h++)
        {
            Vector3 stackPos = pos;
            stackPos.y += h * 2.0f;
            scene->AddTileObject(TileType::WIND_NORTH, stackPos, rot, 1.2f);
        }
    }

    // Center Tower
    Vector3 center = startOffset;
    center.y += 0.0f;
    for (int h = 0; h < 5; h++)
    {
        scene->AddTileObject(TileType::DRAGON_RED, {center.x, center.y + h * 3.0f, center.z}, QuaternionIdentity(), 1.5f);
    }
}
