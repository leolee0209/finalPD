#pragma once
#include <memory>
#include <raylib.h>
#include <raymath.h>
#include "object.hpp"

/**
 * @brief Couples a renderable Model with an Object collider so decorations can participate in collisions.
 */
class CollidableModel
{
public:
    static std::unique_ptr<CollidableModel> Create(Model *model,
                                                   const BoundingBox &localBounds,
                                                   Vector3 position,
                                                   Vector3 scale,
                                                   Vector3 rotationAxis = {0.0f, 1.0f, 0.0f},
                                                   float rotationAngleDeg = 0.0f);

    Model *GetModel() const { return this->model; }
    const Vector3 &GetPosition() const { return this->position; }
    const Vector3 &GetScale() const { return this->scale; }
    const Vector3 &GetRotationAxis() const { return this->rotationAxis; }
    float GetRotationAngleDeg() const { return this->rotationAngleDeg; }
    Object *GetCollider() const { return this->collider.get(); }
    const BoundingBox &GetLocalBounds() const { return this->localBounds; }

    void SetPosition(Vector3 newPosition);
    void SetScale(Vector3 newScale);
    void SetRotation(Vector3 axis, float angleDeg);

private:
    CollidableModel(Model *model,
                    const BoundingBox &localBounds,
                    Vector3 position,
                    Vector3 scale,
                    Vector3 rotationAxis,
                    float rotationAngleDeg);

    void UpdateColliderFromBounds();

    Model *model = nullptr;
    BoundingBox localBounds{};
    Vector3 position{};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    Vector3 rotationAxis{0.0f, 1.0f, 0.0f};
    float rotationAngleDeg = 0.0f;
    std::unique_ptr<Object> collider;
};
