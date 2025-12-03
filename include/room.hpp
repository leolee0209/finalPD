#pragma once

#include <memory>
#include <string>
#include <vector>

#include <raylib.h>
#include <raymath.h>

#include <btBulletCollisionCommon.h>

#include "collidableModel.hpp"
#include "me.hpp"

class Door
{
public:
    struct LeafVisual
    {
        BoundingBox bounds{};
        Vector3 hingeLocal{};
        float targetAngleDeg = 0.0f;
        float currentAngleDeg = 0.0f;
        int meshIndex = -1;
        bool valid = false;
    };

    static std::unique_ptr<Door> Create(std::unique_ptr<CollidableModel> collider,
                                        btCollisionWorld *bulletWorld,
                                        Shader *lightingShader,
                                        float openDurationSeconds,
                                        float openAngleDeg);

    ~Door();

    void Update(float deltaSeconds);
    void Draw() const;
    void Open();
    void Close();
    bool IsOpen() const { return this->openComplete; }
    bool IsClosed() const { return !this->opening && this->openProgress <= 0.0f; }
    void SetLightingShader(Shader *shader);
    bool IsPlayerNearby(const Vector3 &playerPos, float maxDistance = 3.0f) const;

private:
    Door(std::unique_ptr<CollidableModel> collider,
         btCollisionWorld *bulletWorld,
         Shader *lightingShader,
         float openDurationSeconds,
         float openAngleDeg);

    bool InitializeVisuals();
    void ApplyLighting() const;
    void DisableCollision();
    Vector3 TransformPoint(const Vector3 &localPoint) const;
    void DrawLeaf(const LeafVisual &leaf) const;

    std::unique_ptr<CollidableModel> collider;
    Model *visualModel = nullptr;
    btCollisionWorld *bulletWorld = nullptr;
    Shader *lightingShader = nullptr;

    LeafVisual leftLeaf;
    LeafVisual rightLeaf;

    Vector3 basePosition{};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    Vector3 rotationAxis{0.0f, 1.0f, 0.0f};
    float rotationAngleDeg = 0.0f;
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};

    float openDuration = 1.0f;
    float openAngleDeg = 90.0f;
    float openProgress = 0.0f;
    bool opening = false;
    bool openComplete = false;
    bool collisionEnabled = true;
};

enum class RoomType
{
    Start,
    Enemy
};

class Room
{
public:
    Room(std::string name, BoundingBox bounds, RoomType type);

    void AttachDoor(Door *door);
    void Update(const std::vector<Entity *> &enemies);
    bool IsCompleted() const { return this->completed; }
    bool IsPlayerInside(const Vector3 &playerPos) const;
    RoomType GetType() const { return this->type; }
    const BoundingBox &GetBounds() const { return this->bounds; }
    const std::string &GetName() const { return this->name; }
    std::vector<Door *> GetDoors() const { return this->doors; }

private:
    bool ContainsEnemy(const Entity *entity) const;
    void TryOpenDoors();

    std::string name;
    BoundingBox bounds{};
    RoomType type = RoomType::Enemy;
    bool hadEnemies = false;
    bool completed = false;
    std::vector<Door *> doors;
};
