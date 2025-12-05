#include "scene.hpp"
#include <raylib.h>
#include <raymath.h>
#include "constant.hpp"
#include <rlgl.h>
#include "enemyManager.hpp"
#include "dialogBox.hpp"
#include "rlights.hpp"
#include "particle.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <array>
#include <cfloat>
#include <string>
#include "Inventory.hpp"

namespace
{
    constexpr char doorModelPath[] = "decorations/door.glb";
    constexpr float doorOpenAngleDeg = 95.0f;
    constexpr float doorOpenDuration = 1.35f;
    constexpr float doorTargetHeight = 18.0f;
    constexpr float boundingAxisEpsilon = 0.0001f;

    float RandomRange(float minValue, float maxValue)
    {
        if (minValue > maxValue)
            std::swap(minValue, maxValue);
        int minInt = (int)floorf(minValue * 1000.0f);
        int maxInt = (int)floorf(maxValue * 1000.0f);
        int value = GetRandomValue(minInt, maxInt);
        return (float)value / 1000.0f;
    }

    void DrawOutlinedText(const Font &font,
                          const std::string &text,
                          Vector2 position,
                          float fontSize,
                          float outlineThickness,
                          float shadowOffset,
                          Color fill,
                          Color outline,
                          Color shadow)
    {
        DrawTextEx(font, text.c_str(), {position.x + shadowOffset, position.y + shadowOffset}, fontSize, 1.0f, shadow);

        const Vector2 offsets[] = {
            {-1.0f, -1.0f}, {0.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 0.0f}, {1.0f, 0.0f}, {-1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};

        for (Vector2 off : offsets)
        {
            Vector2 outlinePos{position.x + off.x * outlineThickness,
                               position.y + off.y * outlineThickness};
            DrawTextEx(font, text.c_str(), outlinePos, fontSize, 1.0f, outline);
        }

        DrawTextEx(font, text.c_str(), position, fontSize, 1.0f, fill);
    }

    btVector3 ToBtVector(const Vector3 &v)
    {
        return btVector3(v.x, v.y, v.z);
    }

    btQuaternion ToBtQuaternion(const Quaternion &q)
    {
        return btQuaternion(q.x, q.y, q.z, q.w);
    }
}

void DamageIndicatorSystem::Spawn(const Vector3 &worldPosition, float amount)
{
    int rounded = (int)lroundf(amount);
    if (rounded <= 0)
        return;

    DamageIndicator indicator;
    indicator.worldPosition = worldPosition;
    indicator.screenOffset = {RandomRange(-18.0f, 18.0f), RandomRange(-8.0f, 8.0f)};
    indicator.velocity = {RandomRange(-10.0f, 10.0f), RandomRange(28.0f, 46.0f)};
    indicator.lifetime = RandomRange(0.8f, 1.05f);
    indicator.age = 0.0f;
    indicator.text = std::to_string(rounded);
    this->indicators.push_back(std::move(indicator));
}

void DamageIndicatorSystem::Update(float deltaSeconds)
{
    if (this->indicators.empty())
        return;

    for (auto &indicator : this->indicators)
    {
        indicator.age += deltaSeconds;
        indicator.screenOffset.x += indicator.velocity.x * deltaSeconds;
        indicator.screenOffset.y += indicator.velocity.y * deltaSeconds;
    }

    this->indicators.erase(std::remove_if(this->indicators.begin(), this->indicators.end(), [](const DamageIndicator &indicator)
                                          { return indicator.age >= indicator.lifetime; }),
                           this->indicators.end());
}

void DamageIndicatorSystem::Draw(const Camera &camera) const
{
    if (this->indicators.empty())
        return;

    Font font = GetFontDefault();
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    for (const auto &indicator : this->indicators)
    {
        float t = indicator.lifetime > 0.0f ? indicator.age / indicator.lifetime : 1.0f;
        float alpha = 1.0f - t;
        if (alpha <= 0.0f)
            continue;

        Vector2 base = GetWorldToScreen(indicator.worldPosition, camera);
        if (base.x < -96.0f || base.x > screenW + 96.0f ||
            base.y < -96.0f || base.y > screenH + 96.0f)
        {
            continue;
        }

        Vector2 drawPos{base.x + indicator.screenOffset.x, base.y + indicator.screenOffset.y};
        float fontSize = Lerp(38.0f, 26.0f, Clamp(t, 0.0f, 1.0f));
        float outlineThickness = Clamp(fontSize * 0.08f, 1.0f, 5.0f);
        float shadowOffset = Clamp(fontSize * 0.12f, 1.0f, 6.0f);

        unsigned char alphaByte = (unsigned char)Clamp(alpha * 255.0f, 0.0f, 255.0f);
        Color fill = {255, 235, 196, alphaByte};
        Color outline = {40, 5, 5, alphaByte};
        Color shadow = {0, 0, 0, (unsigned char)Clamp(alpha * 200.0f, 0.0f, 255.0f)};

        DrawOutlinedText(font, indicator.text, drawPos, fontSize, outlineThickness, shadowOffset, fill, outline, shadow);
    }
}

void DamageIndicatorSystem::Clear()
{
    this->indicators.clear();
}

// Destructor: unload shared model resources
Scene::~Scene()
{
    this->RemoveDecorationColliders();
    this->decorations.clear();
    this->doors.clear();
    this->rooms.clear();

    // Unload shared briefcase model
    RewardBriefcase::UnloadSharedModel();

    // Only unload GPU resources if the window/context is still active.
    if (IsWindowReady())
    {
        if (this->wallTexture.id != 0)
        {
            UnloadTexture(this->wallTexture);
            this->wallTexture.id = 0;
        }
        if (this->floorTexture.id != 0)
        {
            UnloadTexture(this->floorTexture);
            this->floorTexture.id = 0;
        }
        this->ReleaseDecorationModels();
        this->ShutdownLighting();
        UnloadModel(this->cubeModel);
        UnloadModel(this->sphereModel);
        if (this->glowTexture.id != 0)
        {
            UnloadTexture(this->glowTexture);
            this->glowTexture.id = 0;
        }
    }
    this->damageIndicators.Clear();
    this->ShutdownBulletWorld();
}

void Scene::InitializeBulletWorld()
{
    if (this->bulletWorld)
    {
        return;
    }
    this->bulletConfig = std::make_unique<btDefaultCollisionConfiguration>();
    this->bulletDispatcher = std::make_unique<btCollisionDispatcher>(this->bulletConfig.get());
    this->bulletBroadphase = std::make_unique<btDbvtBroadphase>();
    this->bulletWorld = std::make_unique<btCollisionWorld>(this->bulletDispatcher.get(), this->bulletBroadphase.get(), this->bulletConfig.get());
}

void Scene::ShutdownBulletWorld()
{
    if (!this->bulletWorld)
    {
        return;
    }
    this->RemoveDecorationColliders();
    this->bulletWorld.reset();
    this->bulletBroadphase.reset();
    this->bulletDispatcher.reset();
    this->bulletConfig.reset();
}

void Scene::RemoveDecorationColliders()
{
    if (!this->bulletWorld)
    {
        return;
    }
    for (const auto &decoration : this->decorations)
    {
        if (!decoration)
        {
            continue;
        }
        if (auto *collisionObject = decoration->GetBulletObject())
        {
            this->bulletWorld->removeCollisionObject(collisionObject);
        }
    }
}

btTransform Scene::BuildBtTransform(const Object &obj)
{
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(ToBtVector(obj.getPos()));
    transform.setRotation(ToBtQuaternion(obj.getRotation()));
    return transform;
}

btCollisionShape *Scene::CreateShapeFromObject(const Object &obj)
{
    if (obj.isSphere())
    {
        return new btSphereShape(obj.getSphereRadius());
    }
    Vector3 halfSize = Vector3Scale(obj.getSize(), 0.5f);
    return new btBoxShape(btVector3(halfSize.x, halfSize.y, halfSize.z));
}

namespace
{
    struct DecorationContactCallback : public btCollisionWorld::ContactResultCallback
    {
        explicit DecorationContactCallback(std::vector<CollisionResult> &results) : hits(results)
        {
            this->m_closestDistanceThreshold = 0.0f;
        }

        btScalar addSingleResult(btManifoldPoint &cp,
                                 const btCollisionObjectWrapper *, int, int,
                                 const btCollisionObjectWrapper *, int, int) override
        {
            if (cp.getDistance() > 0.0f)
            {
                return 0.0f;
            }

            CollisionResult result{};
            result.with = nullptr;
            result.collided = true;
            result.penetration = -cp.getDistance();
            btVector3 normal = cp.m_normalWorldOnB;
            Vector3 raylibNormal = {normal.x(), normal.y(), normal.z()};
            if (Vector3Length(raylibNormal) > 0.0f)
            {
                raylibNormal = Vector3Normalize(raylibNormal);
            }
            result.normal = raylibNormal;
            this->hits.push_back(result);
            return 0.0f;
        }

        std::vector<CollisionResult> &hits;
    };
}

void Scene::AppendDecorationCollisions(const Object &obj, std::vector<CollisionResult> &out) const
{
    if (!this->bulletWorld)
    {
        return;
    }

    std::unique_ptr<btCollisionShape> tempShape(Scene::CreateShapeFromObject(obj));
    if (!tempShape)
    {
        return;
    }

    btCollisionObject tempObject;
    tempObject.setCollisionShape(tempShape.get());
    tempObject.setWorldTransform(Scene::BuildBtTransform(obj));

    DecorationContactCallback callback(out);
    this->bulletWorld->contactTest(&tempObject, callback);
}

bool Scene::CheckDecorationCollision(const Object &obj) const
{
    std::vector<CollisionResult> hits;
    hits.reserve(4);
    this->AppendDecorationCollisions(obj, hits);
    return !hits.empty();
}

bool Scene::CheckDecorationSweep(const Vector3 &start, const Vector3 &end, float radius) const
{
    if (!this->bulletWorld)
    {
        return false;
    }

    btSphereShape sphere(radius);
    btTransform from;
    from.setIdentity();
    from.setOrigin(btVector3(start.x, start.y, start.z));
    btTransform to;
    to.setIdentity();
    to.setOrigin(btVector3(end.x, end.y, end.z));

    btCollisionWorld::ClosestConvexResultCallback callback(from.getOrigin(), to.getOrigin());
    callback.m_collisionFilterGroup = btBroadphaseProxy::AllFilter;
    callback.m_collisionFilterMask = btBroadphaseProxy::AllFilter;
    this->bulletWorld->convexSweepTest(&sphere, from, to, callback);
    if (!callback.hasHit())
    {
        return false;
    }

    float sweepLength = Vector3Distance(start, end);
    if (sweepLength < 0.0001f)
    {
        return true;
    }

    float hitDistance = callback.m_closestHitFraction * sweepLength;
    float tolerance = std::max(radius * 0.5f, 0.1f);
    if (hitDistance <= tolerance)
    {
        return false;
    }

    return true;
}

void Scene::ApplyFullTexture(Object &obj, Texture2D &texture)
{
    if (texture.id == 0)
    {
        return;
    }

    obj.useTexture = true;
    obj.texture = &texture;
    obj.sourceRect = {0.0f, 0.0f, (float)texture.width, (float)texture.height};
    obj.tint = WHITE;
}

float Scene::GetFloorTop() const
{
    const Vector3 &floorPos = this->floor.getPos();
    const Vector3 &floorSize = this->floor.getSize();
    return floorPos.y + floorSize.y * 0.5f;
}

Model *Scene::AcquireDecorationModel(const std::string &relativePath)
{
    auto found = this->decorationModelCache.find(relativePath);
    if (found != this->decorationModelCache.end())
    {
        found->second.refCount++;
        if (this->lightingShader.id != 0)
        {
            for (int i = 0; i < found->second.model.materialCount; ++i)
            {
                found->second.model.materials[i].shader = this->lightingShader;
            }
        }
        return &found->second.model;
    }

    CachedModel cacheEntry{};
    cacheEntry.model = LoadModel(relativePath.c_str());
    if (cacheEntry.model.meshCount == 0)
    {
        TraceLog(LOG_WARNING, "Failed to load decoration model: %s", relativePath.c_str());
        UnloadModel(cacheEntry.model);
        return nullptr;
    }

    if (this->lightingShader.id != 0)
    {
        for (int i = 0; i < cacheEntry.model.materialCount; ++i)
        {
            cacheEntry.model.materials[i].shader = this->lightingShader;
        }
    }

    cacheEntry.refCount = 1;
    auto [iter, inserted] = this->decorationModelCache.emplace(relativePath, std::move(cacheEntry));
    return &iter->second.model;
}

void Scene::ReleaseDecorationModels()
{
    for (auto &entry : this->decorationModelCache)
    {
        UnloadModel(entry.second.model);
    }
    this->decorationModelCache.clear();
}

CollidableModel *Scene::AddDecoration(const char *modelPath, Vector3 desiredPosition, float targetHeight, float rotationYDeg, bool addCollision)
{
    Model *model = this->AcquireDecorationModel(modelPath);
    if (model == nullptr)
    {
        return nullptr;
    }

    BoundingBox box = GetModelBoundingBox(*model);
    float currentHeight = box.max.y - box.min.y;
    float uniformScale = (currentHeight > 0.001f) ? targetHeight / currentHeight : 1.0f;
    Vector3 scale = {uniformScale, uniformScale, uniformScale};

    float floorY = this->GetFloorTop();
    desiredPosition.y = floorY - (box.min.y * uniformScale);

    auto decoration = CollidableModel::Create(model, desiredPosition, scale, {0.0f, 1.0f, 0.0f}, rotationYDeg);
    if (!decoration)
    {
        return nullptr;
    }

    if (addCollision)
    {
        auto *bulletObject = decoration->GetBulletObject();
        if (this->bulletWorld && bulletObject)
        {
            this->bulletWorld->addCollisionObject(bulletObject);
        }
    }

    auto *decorationPtr = decoration.get();
    this->decorations.push_back(std::move(decoration));
    return decorationPtr;
}

std::unique_ptr<CollidableModel> Scene::DetachDecoration(CollidableModel *target)
{
    if (!target)
    {
        return nullptr;
    }

    for (auto iter = this->decorations.begin(); iter != this->decorations.end(); ++iter)
    {
        if (iter->get() == target)
        {
            std::unique_ptr<CollidableModel> owned = std::move(*iter);
            this->decorations.erase(iter);
            return owned;
        }
    }
    return nullptr;
}

void Scene::ConfigureDoorPlacement(CollidableModel *door, const Vector3 &desiredCenter)
{
    if (!door)
    {
        return;
    }

    Model *doorModel = door->GetModel();
    if (!doorModel)
    {
        return;
    }

    BoundingBox bounds = GetModelBoundingBox(*doorModel);
    Vector3 scale = door->GetScale();

    Vector3 localCenter = {
        (bounds.min.x + bounds.max.x) * 0.5f,
        (bounds.min.y + bounds.max.y) * 0.5f,
        (bounds.min.z + bounds.max.z) * 0.5f};

    Vector3 scaledCenter = {
        localCenter.x * scale.x,
        localCenter.y * scale.y,
        localCenter.z * scale.z};

    Vector3 axis = door->GetRotationAxis();
    float angle = door->GetRotationAngleDeg();
    if (Vector3Length(axis) > 0.0001f && fabsf(angle) > 0.0001f)
    {
        axis = Vector3Normalize(axis);
        Quaternion rotation = QuaternionFromAxisAngle(axis, angle * DEG2RAD);
        scaledCenter = Vector3RotateByQuaternion(scaledCenter, rotation);
    }

    Vector3 worldPosition = Vector3Subtract(desiredCenter, scaledCenter);
    door->SetPosition(worldPosition);
}

void Scene::InitializeRooms(float roomWidth, float roomLength, float wallHeight,
                            const std::vector<Vector3> &centers)
{
    this->rooms.clear();
    this->rooms.reserve(centers.size());

    for (size_t i = 0; i < centers.size(); ++i)
    {
        const Vector3 &center = centers[i];
        BoundingBox bounds;
        bounds.min = {center.x - roomWidth * 0.5f, 0.0f, center.z - roomLength * 0.5f};
        bounds.max = {center.x + roomWidth * 0.5f, wallHeight, center.z + roomLength * 0.5f};
        RoomType type = (i == 0) ? RoomType::Start : RoomType::Enemy;
        std::string name = (i == 0) ? "Spawn Room" : "Room " + std::to_string(i + 1);
        this->rooms.push_back(std::make_unique<Room>(name, bounds, type));
    }
}

void Scene::BuildDoorNetwork(const std::vector<Vector3> &roomCenters, float roomWidth, float roomLength, float wallThickness)
{
    if (roomCenters.size() < 5)
    {
        return;
    }

    float halfWidth = roomWidth * 0.5f;
    float halfLength = roomLength * 0.5f;
    float doorCenterY = this->GetFloorTop() + doorTargetHeight * 0.5f;

    struct DoorLink
    {
        Vector3 center;
        float rotationDeg;
        int roomA;
        int roomB;
    };

    std::vector<DoorLink> doorLinks;
    doorLinks.reserve(4);
    doorLinks.push_back({{roomCenters[0].x,
                          doorCenterY,
                          roomCenters[0].z + halfLength - wallThickness * 0.5f},
                         0.0f,
                         0,
                         1});
    doorLinks.push_back({{roomCenters[1].x - halfWidth + wallThickness * 0.5f,
                          doorCenterY,
                          roomCenters[1].z},
                         90.0f,
                         1,
                         2});
    doorLinks.push_back({{roomCenters[1].x + halfWidth - wallThickness * 0.5f,
                          doorCenterY,
                          roomCenters[1].z},
                         90.0f,
                         1,
                         3});
    doorLinks.push_back({{roomCenters[3].x,
                          doorCenterY,
                          roomCenters[3].z + halfLength - wallThickness * 0.5f},
                         0.0f,
                         3,
                         4});

    for (const DoorLink &link : doorLinks)
    {
        this->CreateDoorBetweenRooms(link.center, link.rotationDeg, link.roomA, link.roomB);
    }
}

void Scene::CreateDoorBetweenRooms(const Vector3 &doorCenter, float rotationYDeg, int roomA, int roomB)
{
    if (roomA < 0 || roomB < 0)
    {
        return;
    }

    if (roomA >= static_cast<int>(this->rooms.size()) || roomB >= static_cast<int>(this->rooms.size()))
    {
        return;
    }

    CollidableModel *doorDecoration = this->AddDecoration(doorModelPath, doorCenter, doorTargetHeight, rotationYDeg, true);
    if (!doorDecoration)
    {
        return;
    }

    auto owned = this->DetachDecoration(doorDecoration);
    if (!owned)
    {
        return;
    }

    this->ConfigureDoorPlacement(owned.get(), doorCenter);

    Shader *shaderPtr = (this->lightingShader.id != 0) ? &this->lightingShader : nullptr;
    auto door = Door::Create(std::move(owned), this->bulletWorld.get(), shaderPtr, doorOpenDuration, doorOpenAngleDeg);
    if (!door)
    {
        return;
    }

    Door *doorPtr = door.get();
    doorPtr->roomA = this->rooms[roomA].get();
    doorPtr->roomB = this->rooms[roomB].get();
    this->rooms[roomA]->AttachDoor(doorPtr);
    this->rooms[roomB]->AttachDoor(doorPtr);
    this->doors.push_back(std::move(door));
}

void Scene::PopulateRoomEnemies(const std::vector<Vector3> &roomCenters)
{
    // No longer spawns enemies immediately; stored for delayed spawn on room entry
    (void)roomCenters; // Suppress unused parameter warning
}

void Scene::SpawnEnemiesForRoom(Room *room, const Vector3 &roomCenter)
{
    if (!room || room->GetType() != RoomType::Enemy)
    {
        return;
    }

    const Vector3 tileSize = Vector3Scale({44.0f, 60.0f, 30.0f}, 0.06f);
    float floorY = this->GetFloorTop();

    auto placeEnemy = [&](Enemy *enemy, const Vector3 &center, const Vector2 &offset)
    {
        enemy->obj().size = tileSize;
        Vector3 position = {center.x + offset.x, floorY + tileSize.y * 0.5f, center.z + offset.y};
        enemy->obj().pos = position;
        enemy->setPosition(position);
        this->em.addEnemy(enemy);
    };

    // Determine composition based on room name
    std::vector<std::pair<std::string, Vector2>> composition;
    const std::string &name = room->GetName();

    if (name == "Room 2")
    {
        composition = {
            {"vanguard", Vector2{-18.0f, -8.0f}},
            {"support", Vector2{8.0f, 12.0f}}};
    }

    else if (name == "Room 3")
    {
        composition = {
            {"tank", Vector2{0.0f, 0.0f}},
            {"vanguard", Vector2{-10.0f, 8.0f}}};
    }
    else if (name == "Room 4")
    {
        composition = {
            {"sniper", Vector2{-12.0f, 8.0f}},
            {"sniper", Vector2{12.0f, -10.0f}},
            {"summoner", Vector2{0.0f, 0.0f}}};
    }
    else if (name == "Room 5")
    {
        composition = {
            {"tank", Vector2{-16.0f, 10.0f}},
            {"summoner", Vector2{0.0f, -12.0f}},
            {"sniper", Vector2{16.0f, 6.0f}},
            {"support", Vector2{8.0f, 12.0f}}}; // Phase 4: Add support enemy
    }

    for (const auto &entry : composition)
    {
        const std::string &type = entry.first;
        const Vector2 &offset = entry.second;
        if (type == "sniper")
        {
            placeEnemy(new ShooterEnemy(), roomCenter, offset);
        }
        else if (type == "tank")
        {
            placeEnemy(new ChargingEnemy(), roomCenter, offset);
        }
        else if (type == "summoner")
        {
            placeEnemy(new SummonerEnemy(), roomCenter, offset);
        }
        else if (type == "support")
        {
            placeEnemy(new SupportEnemy(), roomCenter, offset);
        }
        else if (type == "vanguard")
        {
            placeEnemy(new VanguardEnemy(), roomCenter, offset);
        }
    }
}

void Scene::AssignEnemyTextures(UIManager *uiManager)
{
    if (!uiManager)
        return;

    // Assign tile textures based on each enemy's associated tile type
    std::vector<Entity *> enemies = this->em.getEntities(ENTITY_ENEMY);
    for (Entity *entity : enemies)
    {
        if (Enemy *enemy = dynamic_cast<Enemy *>(entity))
        {
            TileType type = enemy->getTileType();
            enemy->obj().texture = &uiManager->muim.getSpriteSheet();
            enemy->obj().sourceRect = uiManager->muim.getTile(type);
            enemy->obj().useTexture = true;
        }
    }
}

void Scene::UpdateRooms(const std::vector<Entity *> &enemies)
{
    for (auto &room : this->rooms)
    {
        if (room)
        {
            bool wasCompleted = room->IsCompleted();
            room->Update(enemies);

            // Spawn briefcase when room is newly completed
            if (!wasCompleted && room->IsCompleted() && room->GetType() == RoomType::Enemy)
            {
                // Calculate room center
                BoundingBox bounds = room->GetBounds();
                Vector3 center = {
                    (bounds.min.x + bounds.max.x) * 0.5f,
                    bounds.min.y + 1.0f, // Spawn slightly above floor
                    (bounds.min.z + bounds.max.z) * 0.5f};

                // Generate 3-5 reward tiles with random stats
                Inventory inv;
                auto &tiles = inv.getTiles();
                int tileCount = 3 + (rand() % 3); // 3-5 tiles
                for (int i = 0; i < tileCount; ++i)
                {
                    TileType type = (TileType)(rand() % (int)TileType::TILE_COUNT);
                    float damage = 10.0f + (float)(rand() % 8);
                    float fireRate = 0.9f + ((float)(rand() % 7) / 10.0f);
                    tiles.emplace_back(TileStats(damage, fireRate), type);
                }

                // Create briefcase
                this->rewardBriefcases.push_back(std::make_unique<RewardBriefcase>(center, std::move(inv)));
            }
        }
    }
}

Room *Scene::GetRoomContainingPosition(const Vector3 &pos) const
{
    for (const auto &rptr : this->rooms)
    {
        if (!rptr) continue;
        const BoundingBox &b = rptr->GetBounds();
        if (pos.x >= b.min.x && pos.x <= b.max.x && pos.y >= b.min.y && pos.y <= b.max.y && pos.z >= b.min.z && pos.z <= b.max.z)
        {
            return rptr.get();
        }
    }
    return nullptr;
}

void Scene::DrawDoors() const
{
    for (const auto &door : this->doors)
    {
        if (door)
        {
            door->Draw();
        }
    }
}

void Scene::DrawDecorations() const
{
    for (const auto &decoration : this->decorations)
    {
        if (!decoration)
            continue;
        if (!decoration->GetModel())
            continue;
        DrawModelEx(*decoration->GetModel(), decoration->GetPosition(), decoration->GetRotationAxis(), decoration->GetRotationAngleDeg(), decoration->GetScale(), WHITE);
    }
}

void Scene::InitializeLighting()
{
    // Reset the global light counter before creating new lights
    ResetLights();

    this->lightingShader = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
    if (this->lightingShader.id == 0)
    {
        this->ambientLoc = -1;
        this->viewPosLoc = -1;
        return;
    }

    this->lightingShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(this->lightingShader, "mvp");
    this->lightingShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(this->lightingShader, "matModel");
    this->lightingShader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(this->lightingShader, "matNormal");
    this->viewPosLoc = GetShaderLocation(this->lightingShader, "viewPos");
    this->ambientLoc = GetShaderLocation(this->lightingShader, "ambient");

    TraceLog(LOG_INFO, "Shader locations - viewPos: %d, ambient: %d", this->viewPosLoc, this->ambientLoc);

    if (this->cubeModel.materialCount > 0)
    {
        this->cubeModel.materials[0].shader = this->lightingShader;
    }
    if (this->sphereModel.materialCount > 0)
    {
        this->sphereModel.materials[0].shader = this->lightingShader;
    }

    if (this->ambientLoc >= 0)
    {
        SetShaderValue(this->lightingShader, this->ambientLoc, &this->ambientColor.x, SHADER_UNIFORM_VEC4);
    }
    if (this->viewPosLoc >= 0)
    {
        SetShaderValue(this->lightingShader, this->viewPosLoc, &this->shaderViewPos.x, SHADER_UNIFORM_VEC3);
    }

    for (auto &door : this->doors)
    {
        if (door)
        {
            door->SetLightingShader(&this->lightingShader);
        }
    }
}

void Scene::CreatePointLight(Vector3 position, Color color, float intensity)
{
    if (this->lightingShader.id == 0)
    {
        return;
    }

    float clamped = std::clamp(intensity, 0.0f, 4.0f);
    auto scaleComponent = [clamped](unsigned char channel)
    {
        float scaled = static_cast<float>(channel) * clamped;
        scaled = std::clamp(scaled, 0.0f, 255.0f);
        return static_cast<unsigned char>(scaled);
    };

    Color scaledColor = {scaleComponent(color.r), scaleComponent(color.g), scaleComponent(color.b), color.a};
    Light light = CreateLight(LIGHT_POINT, position, {0.0f, 0.0f, 0.0f}, scaledColor, this->lightingShader);
}

void Scene::ShutdownLighting()
{
    if (this->lightingShader.id != 0)
    {
        UnloadShader(this->lightingShader);
        this->lightingShader.id = 0;
    }
    this->ambientLoc = -1;
    this->viewPosLoc = -1;
}

// Draws a 3D rectangle (cube) for the given object
void Scene::DrawRectangle(const Object &o) const
{
    if (o.isSphere())
    {
        DrawSphereObject(o);
        return;
    }
    if (o.useTexture && o.texture != nullptr)
    {
        // Draw the object using a textured cube
        // We must manually apply the rotation matrix because DrawCubeTextureRec
        // calculates vertices in world space by default.

        rlPushMatrix();
        // 1. Translate to the object's position
        rlTranslatef(o.pos.x, o.pos.y, o.pos.z);

        // 2. Apply the object's rotation
        Vector3 axis;
        float angle;
        o.getRotationAxisAngle(axis, angle);
        rlRotatef(angle, axis.x, axis.y, axis.z);

        // 3. Draw the cube at the local origin (0,0,0)
        // We pass {0,0,0} as the position because we have already moved
        // the drawing context to the object's location using rlTranslatef.
        DrawCubeTextureRec(*o.texture, o.sourceRect, {0.0f, 0.0f, 0.0f}, o.size.x, o.size.y, o.size.z, o.tint);
        rlPopMatrix();
    }
    else
    {
        // Draw the object using a shared cube model scaled to the object's size.
        // DrawModelEx handles translation, rotation (axis + angle) and scale.
        Vector3 axis;
        float angle;
        o.getRotationAxisAngle(axis, angle);
        DrawModelEx(this->cubeModel, o.getPos(), axis, angle, o.getSize(), TOWER_COLOR);

        // Draw wireframe using extended function to match rotation and scale
        DrawModelWiresEx(this->cubeModel, o.getPos(), axis, angle, o.getSize(), DARKBLUE);
    }
}

void Scene::DrawSphereObject(const Object &o) const
{
    float radius = o.getSphereRadius();
    // Draw textured or solid sphere using the sphere model
    Vector3 scale = {radius * 2.0f, radius * 2.0f, radius * 2.0f};

    if (o.useTexture && o.texture != nullptr)
    {
        // Create a temporary material with the texture for this draw call
        Material tempMat = this->sphereModel.materials[0];
        tempMat.maps[MATERIAL_MAP_DIFFUSE].texture = *o.texture;

        // Temporarily replace material, draw, then restore
        Material originalMat = this->sphereModel.materials[0];
        const_cast<Scene *>(this)->sphereModel.materials[0] = tempMat;
        DrawModelEx(this->sphereModel, o.pos, {0.0f, 1.0f, 0.0f}, 0.0f, scale, o.tint);
        const_cast<Scene *>(this)->sphereModel.materials[0] = originalMat;
    }
    else
    {
        // Use the shared sphere model with the lighting shader
        DrawModelEx(this->sphereModel, o.pos, {0.0f, 1.0f, 0.0f}, 0.0f, scale, o.tint);
    }
}

// Draws the entire scene, including the floor, objects, entities, and attacks
void Scene::DrawScene(Camera camera) const
{
    const float floorTop = this->GetFloorTop();
    auto enemyObjects = this->em.getObjects();
    auto projectileObjects = this->am.getObjects();

    // Begin shader mode once for all lit objects
    if (this->lightingShader.id != 0)
    {
        BeginShaderMode(this->lightingShader);
    }

    // Draw floor
    DrawRectangle(this->floor);

    // Draw all static objects in the scene (walls)
    for (auto &o : this->objects)
    {
        if (o && o->isVisible())
            DrawRectangle(*o);
    }

    this->DrawDecorations();
    this->DrawDoors();

    // Draw all reward briefcases
    for (const auto &briefcase : this->rewardBriefcases)
    {
        if (briefcase)
        {
            briefcase->Draw();
        }
    }

    int debugBulletCount = 0;
    for (auto *obj : enemyObjects)
    {
        if (!obj || !obj->isVisible())
            continue;
        if (obj->isSphere())
            debugBulletCount++;
        DrawRectangle(*obj);
    }

    // Draw custom enemy visuals (particles, glows, etc.)
    std::vector<Entity *> enemies = this->em.getEntities(ENTITY_ENEMY);
    for (Entity *entity : enemies)
    {
        if (entity)
        {
            Enemy *enemy = dynamic_cast<Enemy *>(entity);
            if (enemy)
            {
                enemy->Draw();
            }
        }
    }

    // Draw all projectiles managed by the AttackManager (solid core)
    for (const auto &o : projectileObjects)
    {
        if (o && o->isVisible())
            DrawRectangle(*o);
    }

    // End shader mode
    if (this->lightingShader.id != 0)
    {
        EndShaderMode();
    }

    // Draw glow billboards for bullets using additive blending
    if (this->glowTexture.id != 0)
    {
        BeginBlendMode(BLEND_ADDITIVE);
        for (const auto &o : projectileObjects)
        {
            if (o && o->isVisible() && o->isSphere())
            {
                // Draw a billboard slightly larger than the projectile
                DrawBillboard(camera, this->glowTexture, o->getPos(), 1.2f, Color{255, 150, 100, 200});
            }
        }
        // Also draw glow for enemy bullets
        for (auto *obj : enemyObjects)
        {
            if (obj && obj->isVisible() && obj->isSphere())
            {
                DrawBillboard(camera, this->glowTexture, obj->getPos(), 1.2f, Color{255, 150, 100, 200});
            }
        }
        EndBlendMode();
    }

    // Draw a red sun in the sky
    DrawSphere({300.0f, 300.0f, 0.0f}, 100.0f, {255, 0, 0, 255});
    
    // Draw particles (after all other 3D elements)
    this->particles.draw(camera);
}

void Scene::DrawEnemyHealthDialogs(const Camera &camera) const
{
    const std::vector<Entity *> enemies = this->em.getEntities(ENTITY_ENEMY);
    if (enemies.empty())
        return;
    for (Entity *entity : enemies)
    {
        if (!entity || entity->category() != ENTITY_ENEMY)
            continue;

        Enemy *enemy = static_cast<Enemy *>(entity);
        if (!enemy)
            continue;

        DialogBox *dlg = enemy->getHealthDialog();
        if (!dlg)
            continue;

        dlg->Draw(camera);
    }
}

void Scene::DrawDamageIndicators(const Camera &camera) const
{
    this->damageIndicators.Draw(camera);
}

void Scene::EmitDamageIndicator(const Enemy &enemy, float damageAmount)
{
    if (damageAmount <= 0.0f)
        return;

    const Object &body = enemy.obj();
    Vector3 halfSize = Vector3Scale(body.getSize(), 0.5f);

    Vector3 spawn = body.getPos();
    spawn.x += RandomRange(-halfSize.x, halfSize.x);
    spawn.y += halfSize.y + RandomRange(-halfSize.y * 0.2f, halfSize.y * 0.6f);
    spawn.z += RandomRange(-halfSize.z, halfSize.z);

    this->damageIndicators.Spawn(spawn, damageAmount);
}

// Updates all entities and attacks in the scene
void Scene::Update(UpdateContext &uc)
{
    const float deltaSeconds = GetFrameTime();
    
    // Update particle system
    this->particles.update(deltaSeconds);

    // Check if player entered a new room and spawn enemies on first entry
    Room *previousRoom = this->currentPlayerRoom;
    this->currentPlayerRoom = nullptr;
    for (auto &room : this->rooms)
    {
        if (room && room->IsPlayerInside(uc.player->pos()))
        {
            this->currentPlayerRoom = room.get();
            break;
        }
    }

    // Spawn enemies when player enters an enemy room for the first time
    if (this->currentPlayerRoom && this->currentPlayerRoom != previousRoom)
    {
        if (this->currentPlayerRoom->GetType() == RoomType::Enemy)
        {
            if (!this->currentPlayerRoom->AreEnemiesSpawned())
            {
                BoundingBox bounds = this->currentPlayerRoom->GetBounds();
                Vector3 roomCenter = {
                    (bounds.min.x + bounds.max.x) * 0.5f,
                    (bounds.min.y + bounds.max.y) * 0.5f,
                    (bounds.min.z + bounds.max.z) * 0.5f};
                this->SpawnEnemiesForRoom(this->currentPlayerRoom, roomCenter);
                this->currentPlayerRoom->MarkEnemiesSpawned();
                
                // Close all doors to this room to trap enemies inside
                for (Door *door : this->currentPlayerRoom->GetDoors())
                {
                    if (door)
                    {
                        door->Close();
                    }
                }
                
                // Assign textures to newly spawned enemies
                if (uc.uiManager)
                {
                    this->AssignEnemyTextures(uc.uiManager);
                }
            }
        }
    }

    // Update all entities in the scene
    this->em.update(uc);

    // Monitor room completion after enemies update so door logic is in sync
    const std::vector<Entity *> enemies = this->em.getEntities(ENTITY_ENEMY);
    this->UpdateRooms(enemies);

    // Advance any active door animations
    for (auto &door : this->doors)
    {
        if (door)
        {
            door->Update(deltaSeconds);
        }
    }

    // Update all reward briefcases
    for (auto &briefcase : this->rewardBriefcases)
    {
        if (briefcase)
        {
            briefcase->Update(uc);
        }
    }

    // Update all attacks managed by the AttackManager
    this->am.update(uc);

    this->damageIndicators.Update(deltaSeconds);
}

// Constructor initializes the scene with default objects
Scene::Scene()
{
    this->InitializeBulletWorld();

    // Load shared briefcase model once
    RewardBriefcase::LoadSharedModel();
    
    // Initialize particle system
    this->particles.init();
    // Apply global particle tuning: half-size, more intense (brighter)
    this->particles.globalSizeMultiplier = 0.5f;
    this->particles.globalIntensityMultiplier = 1.5f;

    // const Vector3 towerSize = {16.0f, 32.0f, 16.0f}; // Size of the towers
    // const Color towerColor = {150, 200, 200, 255};   // Color of the towers

    // Vector3 towerPos = {16.0f, 16.0f, 16.0f}; // Initial position of the first tower

    // // Add four towers to the scene in a symmetrical pattern
    // this->objects.push_back(new Object(towerSize, towerPos));
    // towerPos.x *= -1;
    // this->objects.push_back(new Object(towerSize, towerPos));
    // towerPos.z *= -1;
    // this->objects.push_back(new Object(towerSize, towerPos));
    // towerPos.x *= -1;
    // this->objects.push_back(new Object(towerSize, towerPos));

    this->wallTexture = LoadTexture("rough_pine_door_4k.blend/textures/rough_pine_door_diff_4k.jpg");
    this->floorTexture = LoadTexture("wood_cabinet_worn_long_4k.blend/textures/wood_cabinet_worn_long_diff_4k.jpg");

    const float roomWidth = 72.0f;  // Reduced to 60% of original (120.0f)
    const float roomLength = 60.0f; // Reduced to 60% of original (100.0f)
    const float wallThickness = 1.0f;
    const float wallHeight = 30.0f;
    const float floorThickness = 0.5f;
    float doorWidth = 12.0f;

    if (Model *doorSample = this->AcquireDecorationModel(doorModelPath))
    {
        BoundingBox sourceBounds = GetModelBoundingBox(*doorSample);
        float sourceHeight = sourceBounds.max.y - sourceBounds.min.y;
        if (sourceHeight > boundingAxisEpsilon)
        {
            float uniformScale = doorTargetHeight / sourceHeight;
            doorWidth = (sourceBounds.max.x - sourceBounds.min.x) * uniformScale;
        }
    }

    const float sharedWidthSpacing = roomWidth - wallThickness;
    const float sharedLengthSpacing = roomLength - wallThickness;

    std::vector<Vector3> roomCenters;
    roomCenters.reserve(5);
    roomCenters.push_back({0.0f, 0.0f, 0.0f});                                                 // Spawn room
    roomCenters.push_back({0.0f, 0.0f, sharedLengthSpacing});                                  // Hub room
    roomCenters.push_back({-sharedWidthSpacing, 0.0f, roomCenters[1].z});                      // West branch
    roomCenters.push_back({sharedWidthSpacing, 0.0f, roomCenters[1].z});                       // East branch
    roomCenters.push_back({sharedWidthSpacing, 0.0f, roomCenters[3].z + sharedLengthSpacing}); // Final room

    Vector3 minBounds{FLT_MAX, 0.0f, FLT_MAX};
    Vector3 maxBounds{-FLT_MAX, 0.0f, -FLT_MAX};
    for (const Vector3 &center : roomCenters)
    {
        minBounds.x = std::min(minBounds.x, center.x - roomWidth * 0.5f);
        maxBounds.x = std::max(maxBounds.x, center.x + roomWidth * 0.5f);
        minBounds.z = std::min(minBounds.z, center.z - roomLength * 0.5f);
        maxBounds.z = std::max(maxBounds.z, center.z + roomLength * 0.5f);
    }

    Vector3 floorSize = {(maxBounds.x - minBounds.x) + wallThickness,
                         floorThickness,
                         (maxBounds.z - minBounds.z) + wallThickness};
    Vector3 floorCenter = {(minBounds.x + maxBounds.x) * 0.5f,
                           -floorThickness / 2.0f,
                           (minBounds.z + maxBounds.z) * 0.5f};

    this->floor = Object(floorSize, floorCenter);
    if (this->floorTexture.id != 0)
    {
        this->ApplyFullTexture(this->floor, this->floorTexture);
    }

    auto createWall = [&](Vector3 size, Vector3 pos)
    {
        Object *wall = new Object(size, pos);
        if (this->wallTexture.id != 0)
        {
            this->ApplyFullTexture(*wall, this->wallTexture);
        }
        this->objects.push_back(wall);
    };

    struct RoomDoorConfig
    {
        bool north = false;
        bool south = false;
        bool east = false;
        bool west = false;
    };

    std::array<RoomDoorConfig, 5> doorConfigs{};
    doorConfigs[0].north = true; // Room 1 -> Room 2
    doorConfigs[1].south = true; // Room 2 -> Room 1
    doorConfigs[1].west = true;  // Room 2 -> Room 3
    doorConfigs[1].east = true;  // Room 2 -> Room 4
    doorConfigs[2].east = true;  // Room 3 -> Room 2
    doorConfigs[3].west = true;  // Room 4 -> Room 2
    doorConfigs[3].north = true; // Room 4 -> Room 5
    doorConfigs[4].south = true; // Room 5 -> Room 4

    auto buildRoom = [&](const Vector3 &center, const RoomDoorConfig &doorConfig)
    {
        float halfWidth = roomWidth * 0.5f;
        float halfLength = roomLength * 0.5f;
        float wallY = wallHeight / 2.0f;

        auto addWallStrip = [&](float zPos, bool hasDoor)
        {
            bool canAddDoor = (doorWidth < roomWidth - 1.0f);
            if (hasDoor && canAddDoor)
            {
                float sideWidth = (roomWidth - doorWidth) * 0.5f;
                float doorHalf = doorWidth * 0.5f;
                float segmentHalf = sideWidth * 0.5f;
                if (sideWidth > 0.1f)
                {
                    createWall({sideWidth, wallHeight, wallThickness},
                               {center.x - (doorHalf + segmentHalf), wallY, zPos});
                    createWall({sideWidth, wallHeight, wallThickness},
                               {center.x + (doorHalf + segmentHalf), wallY, zPos});
                }
            }
            else
            {
                createWall({roomWidth, wallHeight, wallThickness}, {center.x, wallY, zPos});
            }
        };

        addWallStrip(center.z + halfLength - wallThickness / 2.0f, doorConfig.north);
        addWallStrip(center.z - halfLength + wallThickness / 2.0f, doorConfig.south);

        auto addWallColumn = [&](float xPos, bool hasDoor)
        {
            bool canAddDoor = (doorWidth < roomLength - 1.0f);
            if (hasDoor && canAddDoor)
            {
                float sideLength = (roomLength - doorWidth) * 0.5f;
                float doorHalf = doorWidth * 0.5f;
                float segmentHalf = sideLength * 0.5f;
                if (sideLength > 0.1f)
                {
                    createWall({wallThickness, wallHeight, sideLength},
                               {xPos, wallY, center.z - (doorHalf + segmentHalf)});
                    createWall({wallThickness, wallHeight, sideLength},
                               {xPos, wallY, center.z + (doorHalf + segmentHalf)});
                }
            }
            else
            {
                createWall({wallThickness, wallHeight, roomLength}, {xPos, wallY, center.z});
            }
        };

        float eastX = center.x + halfWidth - wallThickness / 2.0f;
        float westX = center.x - halfWidth + wallThickness / 2.0f;
        addWallColumn(eastX, doorConfig.east);
        addWallColumn(westX, doorConfig.west);
    };

    for (size_t i = 0; i < roomCenters.size(); ++i)
    {
        const RoomDoorConfig &config = (i < doorConfigs.size()) ? doorConfigs[i] : RoomDoorConfig{};
        buildRoom(roomCenters[i], config);
    }

    this->InitializeRooms(roomWidth, roomLength, wallHeight, roomCenters);
    this->doors.clear();
    this->BuildDoorNetwork(roomCenters, roomWidth, roomLength, wallThickness);

    // Create a shared unit cube model (unit size) and store it for rendering rotated/scaled objects
    Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    this->cubeModel = LoadModelFromMesh(cubeMesh);

    // Create a shared unit sphere model (unit diameter) for rendering spheres
    Mesh sphereMesh = GenMeshSphere(0.5f, 16, 16);
    this->sphereModel = LoadModelFromMesh(sphereMesh);

    // Generate a soft radial gradient texture for the glow effect
    // Center is White (Hot), Edge is Black (Transparent in Additive Mode)
    Image glowImg = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLACK);
    this->glowTexture = LoadTextureFromImage(glowImg);
    UnloadImage(glowImg);

    this->InitializeLighting();

    // Apply lighting shader to walls after InitializeLighting
    if (this->lightingShader.id != 0)
    {
        for (auto *wall : this->objects)
        {
            if (wall && wall->useTexture && wall->texture != nullptr)
            {
                // Walls don't use the model system, but floor uses cubeModel
                // which already has shader applied in InitializeLighting
            }
        }
    }

    if (this->lightingShader.id != 0)
    {
        for (const Vector3 &center : roomCenters)
        {
            this->CreatePointLight({center.x, 3.0f, center.z}, {255, 214, 180, 255}, 0.24f);
        }
    }

    this->PopulateRoomEnemies(roomCenters);

    // Load decorations directly
    this->AddDecoration("decorations/tables/table_and_chairs/scene.gltf", {-25.0f, 0.0f, 18.0f}, 8.0f, 90.0f);
    this->AddDecoration("decorations/tables/pool_table/scene.gltf", {24.0f, 0.0f, -6.0f}, 4.5f, 12.0f);
    this->AddDecoration("decorations/lights/floor_lamp/scene.gltf", {50.0f, 0.0f, -32.0f}, 13.0f, -25.0f);
    this->AddDecoration("decorations/lights/neon_cactus_lamp/scene.gltf", {-42.0f, 0.0f, -28.0f}, 9.0f, 0.0f);
}

// Getter for the list of objects in the scene
std::vector<Object *> Scene::getStaticObjects() const
{
    return this->objects;
}

std::vector<Entity *> Scene::getEntities(EntityCategory cat)
{
    std::vector<Entity *> r;
    // Query attack manager for projectiles if requested
    auto amEntities = this->am.getEntities(cat);
    r.insert(r.end(), amEntities.begin(), amEntities.end());
    // Query enemy manager for enemies if requested
    auto emEntities = this->em.getEntities(cat);
    r.insert(r.end(), emEntities.begin(), emEntities.end());

    return r;
}

void Scene::SetViewPosition(const Vector3 &viewPosition)
{
    this->shaderViewPos = viewPosition;
    if (this->lightingShader.id != 0 && this->viewPosLoc >= 0)
    {
        SetShaderValue(this->lightingShader, this->viewPosLoc, &this->shaderViewPos.x, SHADER_UNIFORM_VEC3);
    }
}

std::vector<RewardBriefcase *> Scene::GetRewardBriefcases()
{
    std::vector<RewardBriefcase *> result;
    for (auto &briefcase : this->rewardBriefcases)
    {
        if (briefcase)
        {
            result.push_back(briefcase.get());
        }
    }
    return result;
}

void Scene::UpdateRoomDoors(const Vector3 &playerPos)
{
    // Determine current player room
    Room *newRoom = nullptr;
    for (auto &room : this->rooms){
        if (room && room->IsPlayerInside(playerPos))
        {
            newRoom = room.get();
            break;
        }
    }

    // If player changed rooms, close door behind them (unless both rooms are cleared)
    if (newRoom != this->currentPlayerRoom)
    {
        if (this->currentPlayerRoom)
        {
            for (Door *door : this->currentPlayerRoom->GetDoors())
            {
                if (door && door->IsOpen())
                {
                    // Check if both connected rooms are cleared
                    bool bothCleared = false;
                    if (door->GetRoomA() && door->GetRoomB())
                    {
                        bothCleared = door->GetRoomA()->IsCompleted() && door->GetRoomB()->IsCompleted();
                    }
                    
                    // Close door unless both rooms are cleared
                    if (!bothCleared)
                    {
                        door->Close();
                    }
                }
            }
        }
        
        // Also close any doors in the new room that shouldn't be open
        // (in case player somehow skipped past a closed door)
        if (newRoom)
        {
            for (Door *door : newRoom->GetDoors())
            {
                if (door && door->IsOpen())
                {
                    // Check if both connected rooms are cleared
                    bool bothCleared = false;
                    if (door->GetRoomA() && door->GetRoomB())
                    {
                        bothCleared = door->GetRoomA()->IsCompleted() && door->GetRoomB()->IsCompleted();
                    }
                    
                    // Close door unless both rooms are cleared
                    if (!bothCleared)
                    {
                        door->Close();
                    }
                }
            }
        }
    }

    this->currentPlayerRoom = newRoom;
}

void Scene::DrawInteractionPrompts(const Vector3 &playerPos, const Camera &camera) const
{
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const int fontSize = 22;
    int y = screenH - 50;

    // Briefcase prompt (if any nearby and UI not open)
    bool briefcaseNearby = false;
    for (const auto &briefcase : this->rewardBriefcases)
    {
        if (briefcase && briefcase->IsPlayerNearby(playerPos) && !briefcase->IsActivated())
        {
            briefcaseNearby = true;
            break;
        }
    }
    if (briefcaseNearby)
    {
        const char *text = "Press C to Open Briefcase";
        int tw = MeasureText(text, fontSize);
        DrawText(text, (screenW - tw) / 2, y, fontSize, YELLOW);
        y -= 26;
    }

    // Door prompt (if near a closed door in a completed room)
    if (this->currentPlayerRoom && this->currentPlayerRoom->IsCompleted())
    {
        bool doorNearby = false;
        for (Door *door : this->currentPlayerRoom->GetDoors())
        {
            if (door && door->IsClosed() && door->IsPlayerNearby(playerPos, 5.0f))
            {
                doorNearby = true;
                break;
            }
        }
        if (doorNearby)
        {
            const char *text = "Press C to Open Door";
            int tw = MeasureText(text, fontSize);
            DrawText(text, (screenW - tw) / 2, y, fontSize, GREEN);
        }
    }
}

void Scene::DrawCubeTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color) const
{
    float x = position.x;
    float y = position.y;
    float z = position.z;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

    // Vertex data transformation can be defined with the commented lines,
    // but in this example we calculate the transformed vertex data directly when calling rlVertex3f()
    // rlPushMatrix();
    // NOTE: Transformation is applied in inverse order (scale -> rotate -> translate)
    // rlTranslatef(2.0f, 0.0f, 0.0f);
    // rlRotatef(45, 0, 1, 0);
    // rlScalef(2.0f, 2.0f, 2.0f);

    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    // Front Face
    rlNormal3f(0.0f, 0.0f, 1.0f); // Normal Pointing Towards Viewer
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2); // Top Left Of The Texture and Quad
    // Back Face
    rlNormal3f(0.0f, 0.0f, -1.0f); // Normal Pointing Away From Viewer
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2); // Bottom Left Of The Texture and Quad
    // Top Face
    rlNormal3f(0.0f, 1.0f, 0.0f); // Normal Pointing Up
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2); // Top Right Of The Texture and Quad
    // Bottom Face
    rlNormal3f(0.0f, -1.0f, 0.0f); // Normal Pointing Down
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2); // Bottom Right Of The Texture and Quad
    // Right face
    rlNormal3f(1.0f, 0.0f, 0.0f); // Normal Pointing Right
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2); // Bottom Left Of The Texture and Quad
    // Left Face
    rlNormal3f(-1.0f, 0.0f, 0.0f); // Normal Pointing Left
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2); // Top Left Of The Texture and Quad
    rlEnd();
    // rlPopMatrix();

    rlSetTexture(0);
}

// Draw cube with texture piece applied to all faces
void Scene::DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position, float width, float height, float length, Color color) const
{
    float x = position.x;
    float y = position.y;
    float z = position.z;
    float texWidth = (float)texture.width;
    float texHeight = (float)texture.height;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

    BeginBlendMode(BLEND_ALPHA);

    // We calculate the normalized texture coordinates for the desired texture-source-rectangle
    // It means converting from (tex.width, tex.height) coordinates to [0.0f, 1.0f] equivalent
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);

    // Front face
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);

    // Back face
    rlNormal3f(0.0f, 0.0f, -1.0f);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);

    // Top face
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);

    // Bottom face
    rlNormal3f(0.0f, -1.0f, 0.0f);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);

    // Right face
    rlNormal3f(1.0f, 0.0f, 0.0f);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);

    // Left face
    rlNormal3f(-1.0f, 0.0f, 0.0f);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);

    rlEnd();
    EndBlendMode();

    rlSetTexture(0);
}

void Scene::DrawTexturedSphere(Texture2D &texture, const Rectangle &source, const Vector3 &position, float radius, Color tint) const
{
    int rings = 32;
    int slices = 48;
    float texWidth = (float)texture.width;
    float texHeight = (float)texture.height;

    Rectangle src = source;
    if (src.width <= 0.0f || src.height <= 0.0f)
    {
        src = {0.0f, 0.0f, texWidth, texHeight};
    }
    float uBase = src.x / texWidth;
    float vBase = src.y / texHeight;
    float uScale = src.width / texWidth;
    float vScale = src.height / texHeight;

    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlSetTexture(texture.id);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);

    auto emitVertex = [&](float lat, float lon, float uFrac, float vFrac)
    {
        float x = cosf(lon) * cosf(lat);
        float y = sinf(lat);
        float z = sinf(lon) * cosf(lat);
        float u = uBase + uScale * uFrac;
        float v = vBase + vScale * vFrac;
        rlTexCoord2f(u, v);
        rlNormal3f(x, y, z);
        rlVertex3f(x * radius, y * radius, z * radius);
    };

    float latStep = PI / rings;
    float lonStep = (2.0f * PI) / slices;

    for (int ring = 0; ring < rings; ++ring)
    {
        float lat0 = -PI / 2.0f + ring * latStep;
        float lat1 = lat0 + latStep;
        float v0 = (float)ring / rings;
        float v1 = (float)(ring + 1) / rings;
        for (int slice = 0; slice < slices; ++slice)
        {
            float lon0 = slice * lonStep;
            float lon1 = lon0 + lonStep;
            float u0 = (float)slice / slices;
            float u1 = (float)(slice + 1) / slices;

            emitVertex(lat0, lon0, u0, v0);
            emitVertex(lat1, lon0, u0, v1);
            emitVertex(lat1, lon1, u1, v1);

            emitVertex(lat0, lon0, u0, v0);
            emitVertex(lat1, lon1, u1, v1);
            emitVertex(lat0, lon1, u1, v0);
        }
    }

    rlEnd();
    rlSetTexture(0);
    rlPopMatrix();
}