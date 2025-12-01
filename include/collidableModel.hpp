#pragma once
#include <memory>
#include <vector>
#include <raylib.h>
#include <raymath.h>
#include <btBulletCollisionCommon.h>

/**
 * @brief Couples a renderable Model with a Bullet collision object built from its mesh data.
 */
class CollidableModel
{
public:
    static std::unique_ptr<CollidableModel> Create(Model *model,
                                                   Vector3 position,
                                                   Vector3 scale,
                                                   Vector3 rotationAxis = {0.0f, 1.0f, 0.0f},
                                                   float rotationAngleDeg = 0.0f);

    Model *GetModel() const { return this->model; }
    const Vector3 &GetPosition() const { return this->position; }
    const Vector3 &GetScale() const { return this->scale; }
    const Vector3 &GetRotationAxis() const { return this->rotationAxis; }
    float GetRotationAngleDeg() const { return this->rotationAngleDeg; }
    btCollisionObject *GetBulletObject() const { return this->collisionObject.get(); }

    void SetPosition(Vector3 newPosition);
    void SetScale(Vector3 newScale);
    void SetRotation(Vector3 axis, float angleDeg);

private:
    struct MeshBuffers
    {
        std::vector<int> indices;
        std::vector<btScalar> vertices;
    };

    CollidableModel(Model *model,
                    Vector3 position,
                    Vector3 scale,
                    Vector3 rotationAxis,
                    float rotationAngleDeg);

    bool BuildMeshShape(Model *model);
    void UpdateTransform();
    void UpdateScale();
    static MeshBuffers CopyMeshData(const Mesh &mesh);
    static bool ValidateMesh(const Mesh &mesh);

    Model *model = nullptr;
    Vector3 position{};
    Vector3 scale{1.0f, 1.0f, 1.0f};
    Vector3 rotationAxis{0.0f, 1.0f, 0.0f};
    float rotationAngleDeg = 0.0f;
    std::vector<MeshBuffers> meshBuffers;
    std::unique_ptr<btTriangleIndexVertexArray> meshInterface;
    std::unique_ptr<btBvhTriangleMeshShape> meshShape;
    std::unique_ptr<btCollisionObject> collisionObject;
};
