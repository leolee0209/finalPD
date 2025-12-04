#include "room.hpp"

#include <algorithm>

#include <rlgl.h>

namespace
{
    constexpr float kBoundingAxisEpsilon = 0.0001f;
}

// ---------------- Door ----------------

std::unique_ptr<Door> Door::Create(std::unique_ptr<CollidableModel> collider,
                                   btCollisionWorld *bulletWorld,
                                   Shader *lightingShader,
                                   float openDurationSeconds,
                                   float openAngleDeg)
{
    if (!collider)
    {
        return nullptr;
    }

    std::unique_ptr<Door> door(new Door(std::move(collider), bulletWorld, lightingShader, openDurationSeconds, openAngleDeg));
    if (!door->InitializeVisuals())
    {
        return nullptr;
    }
    door->ApplyLighting();
    return door;
}

Door::Door(std::unique_ptr<CollidableModel> doorCollider,
           btCollisionWorld *bulletWorldPtr,
           Shader *shaderPtr,
           float openDurationSeconds,
           float openAngleDegrees)
    : collider(std::move(doorCollider)),
      bulletWorld(bulletWorldPtr),
      lightingShader(shaderPtr),
      openDuration(openDurationSeconds),
      openAngleDeg(openAngleDegrees)
{
    if (this->collider)
    {
        this->basePosition = this->collider->GetPosition();
        this->scale = this->collider->GetScale();
        this->rotationAxis = this->collider->GetRotationAxis();
        this->rotationAngleDeg = this->collider->GetRotationAngleDeg();
        Vector3 axis = this->rotationAxis;
        if (Vector3Length(axis) < kBoundingAxisEpsilon)
        {
            axis = {0.0f, 1.0f, 0.0f};
        }
        axis = Vector3Normalize(axis);
        this->rotation = QuaternionFromAxisAngle(axis, this->rotationAngleDeg * DEG2RAD);
    }
}

Door::~Door()
{
    this->DisableCollision();
}

bool Door::InitializeVisuals()
{
    if (!this->collider)
    {
        return false;
    }

    this->visualModel = this->collider->GetModel();
    if (!this->visualModel)
    {
        return false;
    }

    if (this->visualModel->meshCount < 2)
    {
        TraceLog(LOG_WARNING, "Door model missing expected meshes");
        this->visualModel = nullptr;
        return false;
    }

    auto setupLeaf = [&](LeafVisual &leaf, int meshIndex, float openSign)
    {
        if (!this->visualModel || meshIndex < 0 || meshIndex >= this->visualModel->meshCount)
        {
            return;
        }

        leaf.meshIndex = meshIndex;
        leaf.bounds = GetMeshBoundingBox(this->visualModel->meshes[meshIndex]);
        float hingeX = (openSign < 0.0f) ? leaf.bounds.min.x : leaf.bounds.max.x;
        float centerZ = (leaf.bounds.min.z + leaf.bounds.max.z) * 0.5f;
        leaf.hingeLocal = {hingeX, leaf.bounds.min.y, centerZ};
        leaf.targetAngleDeg = this->openAngleDeg * openSign;
        leaf.currentAngleDeg = 0.0f;
        leaf.valid = true;
    };

    setupLeaf(this->leftLeaf, 0, -1.0f);
    setupLeaf(this->rightLeaf, 1, 1.0f);

    if (!this->leftLeaf.valid || !this->rightLeaf.valid)
    {
        this->visualModel = nullptr;
        return false;
    }

    return true;
}

void Door::ApplyLighting() const
{
    if (!this->visualModel || !this->lightingShader)
    {
        return;
    }

    for (int i = 0; i < this->visualModel->materialCount; ++i)
    {
        this->visualModel->materials[i].shader = *this->lightingShader;
    }
}

void Door::DisableCollision()
{
    if (!this->collisionEnabled || !this->bulletWorld || !this->collider)
    {
        return;
    }

    if (btCollisionObject *object = this->collider->GetBulletObject())
    {
        this->bulletWorld->removeCollisionObject(object);
    }
    this->collisionEnabled = false;
}

void Door::Update(float deltaSeconds)
{
    if (!this->opening || this->openComplete)
    {
        return;
    }

    if (this->openDuration <= 0.0f)
    {
        this->openProgress = 1.0f;
    }
    else
    {
        this->openProgress = std::clamp(this->openProgress + deltaSeconds / this->openDuration, 0.0f, 1.0f);
    }

    float eased = this->openProgress * this->openProgress * (3.0f - 2.0f * this->openProgress);
    if (this->leftLeaf.valid)
    {
        this->leftLeaf.currentAngleDeg = this->leftLeaf.targetAngleDeg * eased;
    }
    if (this->rightLeaf.valid)
    {
        this->rightLeaf.currentAngleDeg = this->rightLeaf.targetAngleDeg * eased;
    }

    if (this->openProgress >= 1.0f)
    {
        this->openComplete = true;
        this->DisableCollision();
    }
}

void Door::Draw() const
{
    if (!this->visualModel)
    {
        return;
    }

    this->DrawLeaf(this->leftLeaf);
    this->DrawLeaf(this->rightLeaf);
}

void Door::Open()
{
    if (this->openComplete)
    {
        return;
    }
    this->opening = true;
}

void Door::Close()
{
    this->opening = false;
    this->openComplete = false;
    this->openProgress = 0.0f;
    if (this->leftLeaf.valid)
    {
        this->leftLeaf.currentAngleDeg = 0.0f;
    }
    if (this->rightLeaf.valid)
    {
        this->rightLeaf.currentAngleDeg = 0.0f;
    }
    
    // Re-enable collision
    if (!this->collisionEnabled && this->bulletWorld && this->collider)
    {
        if (btCollisionObject *object = this->collider->GetBulletObject())
        {
            this->bulletWorld->addCollisionObject(object);
            this->collisionEnabled = true;
        }
    }
}

bool Door::IsPlayerNearby(const Vector3 &playerPos, float maxDistance) const
{
    if (!this->collider)
    {
        return false;
    }
    
    Vector3 doorPos = this->collider->GetPosition();
    float distSq = Vector3DistanceSqr(playerPos, doorPos);
    return distSq <= (maxDistance * maxDistance);
}

void Door::SetLightingShader(Shader *shader)
{
    this->lightingShader = shader;
    this->ApplyLighting();
}

Vector3 Door::TransformPoint(const Vector3 &localPoint) const
{
    Vector3 scaled = {localPoint.x * this->scale.x, localPoint.y * this->scale.y, localPoint.z * this->scale.z};
    if (fabsf(this->rotationAngleDeg) > kBoundingAxisEpsilon)
    {
        scaled = Vector3RotateByQuaternion(scaled, this->rotation);
    }
    return Vector3Add(this->basePosition, scaled);
}

void Door::DrawLeaf(const LeafVisual &leaf) const
{
    if (!leaf.valid || !this->visualModel)
    {
        return;
    }

    if (leaf.meshIndex < 0 || leaf.meshIndex >= this->visualModel->meshCount)
    {
        return;
    }

    int materialIndex = 0;
    if (this->visualModel->meshMaterial)
    {
        int candidate = this->visualModel->meshMaterial[leaf.meshIndex];
        if (candidate >= 0 && candidate < this->visualModel->materialCount)
        {
            materialIndex = candidate;
        }
    }

    Vector3 hingeWorld = this->TransformPoint(leaf.hingeLocal);

    rlPushMatrix();
    rlTranslatef(hingeWorld.x, hingeWorld.y, hingeWorld.z);
    if (fabsf(this->rotationAngleDeg) > kBoundingAxisEpsilon)
    {
        rlRotatef(this->rotationAngleDeg, this->rotationAxis.x, this->rotationAxis.y, this->rotationAxis.z);
    }
    rlScalef(this->scale.x, this->scale.y, this->scale.z);
    rlRotatef(leaf.currentAngleDeg, 0.0f, 1.0f, 0.0f);
    rlTranslatef(-leaf.hingeLocal.x, -leaf.hingeLocal.y, -leaf.hingeLocal.z);
    DrawMesh(this->visualModel->meshes[leaf.meshIndex], this->visualModel->materials[materialIndex], MatrixIdentity());
    rlPopMatrix();
}

// ---------------- Room ----------------

Room::Room(std::string roomName, BoundingBox roomBounds, RoomType roomType)
    : name(std::move(roomName)),
      bounds(roomBounds),
      type(roomType)
{
    if (this->type == RoomType::Start)
    {
        this->completed = true;
    }
}

void Room::AttachDoor(Door *door)
{
    if (!door)
    {
        return;
    }

    this->doors.push_back(door);
    if (this->completed)
    {
        // Do not auto-open doors; require player interaction
    }
}

void Room::Update(const std::vector<Entity *> &enemies)
{
    if (this->type == RoomType::Start || this->completed)
    {
        return;
    }

    bool hasEnemy = false;
    for (Entity *entity : enemies)
    {
        if (!entity || entity->category() != ENTITY_ENEMY)
        {
            continue;
        }
        if (this->ContainsEnemy(entity))
        {
            hasEnemy = true;
            this->hadEnemies = true;
            break;
        }
    }

    if (!hasEnemy && this->hadEnemies)
    {
        this->completed = true;
        this->TryOpenDoors();
    }
}

bool Room::ContainsEnemy(const Entity *entity) const
{
    if (!entity)
    {
        return false;
    }

    const Vector3 pos = entity->obj().getPos();
    return pos.x >= this->bounds.min.x && pos.x <= this->bounds.max.x &&
           pos.y >= this->bounds.min.y && pos.y <= this->bounds.max.y &&
           pos.z >= this->bounds.min.z && pos.z <= this->bounds.max.z;
}

bool Room::IsPlayerInside(const Vector3 &playerPos) const
{
    return playerPos.x >= this->bounds.min.x && playerPos.x <= this->bounds.max.x &&
           playerPos.y >= this->bounds.min.y && playerPos.y <= this->bounds.max.y &&
           playerPos.z >= this->bounds.min.z && playerPos.z <= this->bounds.max.z;
}

void Room::TryOpenDoors()
{
    for (Door *door : this->doors)
    {
        if (door)
        {
            // Do not auto-open doors; require player interaction
        }
    }
}
