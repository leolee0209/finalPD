#include "scene.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <rlgl.h>
#include "enemyManager.hpp"
#include "rlights.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace
{
btVector3 ToBtVector(const Vector3 &v)
{
    return btVector3(v.x, v.y, v.z);
}

btQuaternion ToBtQuaternion(const Quaternion &q)
{
    return btQuaternion(q.x, q.y, q.z, q.w);
}
}
// Destructor: unload shared model resources
Scene::~Scene()
{
    this->RemoveDecorationColliders();
    this->decorations.clear();
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
    }
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

void Scene::DrawPlanarShadow(const Object &o, float floorY) const
{
    if (!o.isVisible())
    {
        return;
    }

    const Vector3 &objPos = o.getPos();
    if (objPos.y < floorY - 0.05f)
    {
        return;
    }

    float heightAboveFloor = std::max(0.0f, objPos.y - floorY);
    float fade = std::clamp(1.0f - heightAboveFloor * 0.05f, this->shadowMinAlpha, 1.0f);
    float baseAlpha = static_cast<float>(this->shadowColor.a) / 255.0f;
    float finalAlpha = std::clamp(baseAlpha * fade, this->shadowMinAlpha, 1.0f);
    Color finalColor = this->shadowColor;
    finalColor.a = static_cast<unsigned char>(finalAlpha * 255.0f);

    Vector3 shadowPos = {objPos.x, floorY - (this->shadowThickness * 0.5f) - 0.005f, objPos.z};

    if (o.isSphere())
    {
        float radius = o.getSphereRadius() * this->shadowInflation;
        DrawCylinder(shadowPos, radius, radius, this->shadowThickness, 16, finalColor);
    }
    else
    {
        Vector3 size = o.getSize();
        float width = std::max(0.1f, size.x * this->shadowInflation);
        float length = std::max(0.1f, size.z * this->shadowInflation);
        DrawCube(shadowPos, width, this->shadowThickness, length, finalColor);
    }
}

void Scene::DrawShadowCollection(const std::vector<Object *> &items, float floorY) const
{
    for (auto *obj : items)
    {
        if (obj)
        {
            this->DrawPlanarShadow(*obj, floorY);
        }
    }
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

void Scene::AddDecoration(const char *modelPath, Vector3 desiredPosition, float targetHeight, float rotationYDeg)
{
    Model *model = this->AcquireDecorationModel(modelPath);
    if (model == nullptr)
    {
        return;
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
        return;
    }

    auto *bulletObject = decoration->GetBulletObject();
    if (this->bulletWorld && bulletObject)
    {
        this->bulletWorld->addCollisionObject(bulletObject);
    }

    this->decorations.push_back(std::move(decoration));
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

    if (this->cubeModel.materialCount > 0)
    {
        this->cubeModel.materials[0].shader = this->lightingShader;
    }

    if (this->ambientLoc >= 0)
    {
        SetShaderValue(this->lightingShader, this->ambientLoc, &this->ambientColor.x, SHADER_UNIFORM_VEC4);
    }
    if (this->viewPosLoc >= 0)
    {
        SetShaderValue(this->lightingShader, this->viewPosLoc, &this->shaderViewPos.x, SHADER_UNIFORM_VEC3);
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
    CreateLight(LIGHT_POINT, position, {0.0f, 0.0f, 0.0f}, scaledColor, this->lightingShader);
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
    if (o.useTexture && o.texture != nullptr)
    {
        this->DrawTexturedSphere(*o.texture, o.sourceRect, o.pos, radius, o.tint);
    }
    else
    {
        DrawSphereEx(o.pos, radius, 24, 32, o.tint);
        DrawSphereWires(o.pos, radius, 16, 16, DARKBLUE);
    }
}

// Draws the entire scene, including the floor, objects, entities, and attacks
void Scene::DrawScene() const
{
    const float floorTop = this->GetFloorTop();
    auto enemyObjects = this->em.getObjects();
    auto projectileObjects = this->am.getObjects();

    if (this->lightingShader.id != 0)
    {
        BeginShaderMode(this->lightingShader);
        DrawRectangle(this->floor);
        EndShaderMode();
    }
    else
    {
        DrawRectangle(this->floor);
    }

    this->DrawShadowCollection(enemyObjects, floorTop);
    this->DrawShadowCollection(projectileObjects, floorTop);

    if (this->lightingShader.id != 0)
    {
        BeginShaderMode(this->lightingShader);
    }

    // Draw all static objects in the scene
    for (auto &o : this->objects)
    {
        if (o && o->isVisible())
            DrawRectangle(*o);
    }

    this->DrawDecorations();

    int debugBulletCount = 0;
    for (auto *obj : enemyObjects)
    {
        if (!obj || !obj->isVisible())
            continue;
        if (obj->isSphere())
            debugBulletCount++;
        DrawRectangle(*obj);
    }

    // Draw all projectiles managed by the AttackManager
    for (const auto &o : projectileObjects)
    {
        if (o && o->isVisible())
            DrawRectangle(*o);
    }

    if (this->lightingShader.id != 0)
    {
        EndShaderMode();
    }

    // Draw a red sun in the sky
    DrawSphere({300.0f, 300.0f, 0.0f}, 100.0f, {255, 0, 0, 255});
}

void Scene::DrawEnemyHealthDialogs(const Camera &camera) const
{
    const std::vector<Entity *> enemies = this->em.getEntities(ENTITY_ENEMY);
    if (enemies.empty())
        return;

    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    if (Vector3LengthSqr(camForward) < 0.0001f)
    {
        camForward = {0.0f, 0.0f, -1.0f};
    }
    Vector3 camRight = Vector3CrossProduct(camForward, {0.0f, 1.0f, 0.0f});
    if (Vector3LengthSqr(camRight) < 0.0001f)
    {
        camRight = {1.0f, 0.0f, 0.0f};
    }
    else
    {
        camRight = Vector3Normalize(camRight);
    }
    Vector3 camUp = Vector3CrossProduct(camRight, camForward);
    if (Vector3LengthSqr(camUp) < 0.0001f)
    {
        camUp = {0.0f, 1.0f, 0.0f};
    }
    else
    {
        camUp = Vector3Normalize(camUp);
    }

    const float worldBarWidth = 1.7f;
    const float worldBarHeight = 0.32f;
    const float barVisibleDistance = 55.0f;
    const float numbersVisibleDistance = 32.0f;
    const float verticalOffset = 1.4f;
    const Color barOutline{0, 0, 0, 220};
    const Color barBackground{30, 30, 36, 220};
    const Color barFill{230, 41, 55, 255};

    for (Entity *entity : enemies)
    {
        if (!entity || entity->category() != ENTITY_ENEMY)
            continue;

        Enemy *enemy = static_cast<Enemy *>(entity);
        if (!enemy)
            continue;

        const int currentHealth = enemy->getHealth();
        const int maxHealth = enemy->getMaxHealth();
        if (maxHealth <= 0 || currentHealth <= 0)
            continue;

        Vector3 headPos = enemy->obj().getPos();
        headPos.y += enemy->obj().getSize().y * 0.5f + verticalOffset;

        Vector3 toEnemy = Vector3Subtract(headPos, camera.position);
        float distSq = Vector3LengthSqr(toEnemy);
        if (distSq < 0.0001f)
            continue;
        float dist = sqrtf(distSq);
        if (dist > barVisibleDistance)
            continue;
        Vector3 toEnemyDir = Vector3Scale(toEnemy, 1.0f / sqrtf(distSq));
        if (Vector3DotProduct(camForward, toEnemyDir) <= 0.0f)
            continue;

        Vector2 screenPos = GetWorldToScreen(headPos, camera);
        if (screenPos.x < 0.0f || screenPos.x > static_cast<float>(GetScreenWidth()) ||
            screenPos.y < 0.0f || screenPos.y > static_cast<float>(GetScreenHeight()))
        {
            continue;
        }

        Vector3 barCenter = headPos;
        Vector3 halfRight = Vector3Scale(camRight, worldBarWidth * 0.5f);
        Vector3 halfUp = Vector3Scale(camUp, worldBarHeight * 0.5f);

        Vector2 screenLeft = GetWorldToScreen(Vector3Subtract(barCenter, halfRight), camera);
        Vector2 screenRight = GetWorldToScreen(Vector3Add(barCenter, halfRight), camera);
        Vector2 screenTop = GetWorldToScreen(Vector3Add(barCenter, halfUp), camera);
        Vector2 screenBottom = GetWorldToScreen(Vector3Subtract(barCenter, halfUp), camera);

        float pixelWidth = Vector2Distance(screenLeft, screenRight);
        float pixelHeight = fabsf(screenTop.y - screenBottom.y);
        if (pixelWidth < 4.0f || pixelHeight < 2.0f)
            continue;

        Rectangle barBack{screenPos.x - pixelWidth * 0.5f,
                          screenPos.y - pixelHeight,
                          pixelWidth,
                          pixelHeight};
        DrawRectangleRounded(barBack, 0.45f, 6, barBackground);
        DrawRectangleRoundedLines(barBack, 0.45f, 6, barOutline);

        float fillPercent = enemy->getHealthPercent();
        if (fillPercent > 0.0f)
        {
            Rectangle barFillRect = barBack;
            barFillRect.width = barBack.width * fillPercent;
            DrawRectangleRounded(barFillRect, 0.4f, 6, barFill);
        }

        if (dist <= numbersVisibleDistance)
        {
            const char *healthText = TextFormat("%d / %d", currentHealth, maxHealth);
            int fontSize = static_cast<int>(Clamp(pixelHeight * 0.75f, 10.0f, 30.0f));
            if (fontSize < 10)
                fontSize = 10;
            int textWidth = MeasureText(healthText, fontSize);
            float textX = barBack.x + barBack.width * 0.5f - textWidth * 0.5f;
            float textY = barBack.y + barBack.height * 0.5f - fontSize * 0.5f;
            DrawText(healthText, static_cast<int>(textX), static_cast<int>(textY), fontSize, RAYWHITE);
        }
    }
}

// Updates all entities and attacks in the scene
void Scene::Update(UpdateContext &uc)
{
    // Update all entities in the scene
    this->em.update(uc);

    // Update all attacks managed by the AttackManager
    this->am.update(uc);
}

// Constructor initializes the scene with default objects
Scene::Scene()
{
    this->InitializeBulletWorld();
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

    const float roomWidth = 120.0f;
    const float roomLength = 100.0f;
    const float wallThickness = 1.0f;
    const float wallHeight = 30.0f;
    const float floorThickness = 0.5f;

    auto createWall = [&](Vector3 size, Vector3 pos)
    {
        Object *wall = new Object(size, pos);
        if (this->wallTexture.id != 0)
        {
            this->ApplyFullTexture(*wall, this->wallTexture);
        }
        this->objects.push_back(wall);
    };

    createWall({roomWidth, wallHeight, wallThickness}, {0.0f, wallHeight / 2.0f, roomLength / 2.0f - wallThickness / 2.0f});
    createWall({roomWidth, wallHeight, wallThickness}, {0.0f, wallHeight / 2.0f, -roomLength / 2.0f + wallThickness / 2.0f});
    createWall({wallThickness, wallHeight, roomLength}, {roomWidth / 2.0f - wallThickness / 2.0f, wallHeight / 2.0f, 0.0f});
    createWall({wallThickness, wallHeight, roomLength}, {-roomWidth / 2.0f + wallThickness / 2.0f, wallHeight / 2.0f, 0.0f});

    this->floor = Object({roomWidth - wallThickness, floorThickness, roomLength - wallThickness},
                         {0.0f, -floorThickness / 2.0f, 0.0f});
    if (this->floorTexture.id != 0)
    {
        this->ApplyFullTexture(this->floor, this->floorTexture);
    }

    // Create a shared unit cube model (unit size) and store it for rendering rotated/scaled objects
    Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    this->cubeModel = LoadModelFromMesh(cubeMesh);

    this->InitializeLighting();

    if (this->lightingShader.id != 0)
    {
        this->CreatePointLight({0.0f, 3, 0.0f}, {255, 214, 170, 255}, 0.2f);
        // this->CreatePointLight({0.0f, wallHeight - 2.0f, 0.0f}, {255, 214, 170, 255}, 0.35f);
        // this->CreatePointLight({roomWidth / 2.5f, wallHeight * 0.8f, roomLength / 2.5f}, {255, 190, 140, 255}, 0.3f);
        // this->CreatePointLight({-roomWidth / 2.5f, wallHeight * 0.8f, -roomLength / 2.5f}, {255, 220, 190, 255}, 0.25f);
    }

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