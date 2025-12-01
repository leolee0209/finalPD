#include "collidableModel.hpp"
#include <cmath>

namespace
{
Vector3 MultiplyComponents(Vector3 a, Vector3 b)
{
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}
}

std::unique_ptr<CollidableModel> CollidableModel::Create(Model *model,
                                                          const BoundingBox &localBounds,
                                                          Vector3 position,
                                                          Vector3 scale,
                                                          Vector3 rotationAxis,
                                                          float rotationAngleDeg)
{
    if (model == nullptr)
    {
        return nullptr;
    }

    auto instance = std::unique_ptr<CollidableModel>(new CollidableModel(model, localBounds, position, scale, rotationAxis, rotationAngleDeg));
    instance->UpdateColliderFromBounds();
    return instance;
}

CollidableModel::CollidableModel(Model *model,
                                 const BoundingBox &localBounds,
                                 Vector3 position,
                                 Vector3 scale,
                                 Vector3 rotationAxis,
                                 float rotationAngleDeg)
    : model(model), localBounds(localBounds), position(position), scale(scale), rotationAxis(rotationAxis), rotationAngleDeg(rotationAngleDeg), collider(std::make_unique<Object>())
{
    this->collider->setVisible(false);
}

void CollidableModel::SetPosition(Vector3 newPosition)
{
    this->position = newPosition;
    this->UpdateColliderFromBounds();
}

void CollidableModel::SetScale(Vector3 newScale)
{
    this->scale = newScale;
    this->UpdateColliderFromBounds();
}

void CollidableModel::SetRotation(Vector3 axis, float angleDeg)
{
    this->rotationAxis = axis;
    this->rotationAngleDeg = angleDeg;
    this->UpdateColliderFromBounds();
}

void CollidableModel::UpdateColliderFromBounds()
{
    if (!this->collider)
    {
        return;
    }

    Vector3 boundsSize = {
        this->localBounds.max.x - this->localBounds.min.x,
        this->localBounds.max.y - this->localBounds.min.y,
        this->localBounds.max.z - this->localBounds.min.z};

    Vector3 scaledSize = MultiplyComponents(boundsSize, this->scale);
    Vector3 boundsCenter = {
        (this->localBounds.min.x + this->localBounds.max.x) * 0.5f,
        (this->localBounds.min.y + this->localBounds.max.y) * 0.5f,
        (this->localBounds.min.z + this->localBounds.max.z) * 0.5f};
    Vector3 scaledCenter = MultiplyComponents(boundsCenter, this->scale);
    Vector3 worldCenter = Vector3Add(this->position, scaledCenter);

    this->collider->size = scaledSize;
    this->collider->pos = worldCenter;
    this->collider->setAsBox(scaledSize);

    Vector3 axis = this->rotationAxis;
    if (Vector3Length(axis) < 1e-4f)
    {
        axis = {0.0f, 1.0f, 0.0f};
    }
    axis = Vector3Normalize(axis);

    this->collider->setRotation(QuaternionFromAxisAngle(axis, this->rotationAngleDeg * DEG2RAD));
    this->collider->UpdateOBB();
    this->collider->setVisible(false);
}
