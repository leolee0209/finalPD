#include "collidableModel.hpp"
#include <cmath>

namespace
{
btQuaternion QuaternionFromAxisAngleSafe(Vector3 axis, float angleDeg)
{
    if (Vector3Length(axis) < 1e-4f)
    {
        axis = {0.0f, 1.0f, 0.0f};
    }
    axis = Vector3Normalize(axis);
    float angleRad = angleDeg * DEG2RAD;
    return btQuaternion(axis.x * sinf(angleRad / 2.0f),
                        axis.y * sinf(angleRad / 2.0f),
                        axis.z * sinf(angleRad / 2.0f),
                        cosf(angleRad / 2.0f));
}
}

std::unique_ptr<CollidableModel> CollidableModel::Create(Model *model,
                                                          Vector3 position,
                                                          Vector3 scale,
                                                          Vector3 rotationAxis,
                                                          float rotationAngleDeg)
{
    if (model == nullptr)
    {
        return nullptr;
    }

    auto instance = std::unique_ptr<CollidableModel>(new CollidableModel(model, position, scale, rotationAxis, rotationAngleDeg));
    if (!instance->BuildMeshShape(model))
    {
        return nullptr;
    }

    instance->UpdateScale();
    instance->UpdateTransform();
    return instance;
}

CollidableModel::CollidableModel(Model *modelIn,
                                 Vector3 positionIn,
                                 Vector3 scaleIn,
                                 Vector3 rotationAxisIn,
                                 float rotationAngleDegIn)
    : model(modelIn), position(positionIn), scale(scaleIn), rotationAxis(rotationAxisIn), rotationAngleDeg(rotationAngleDegIn)
{
}

void CollidableModel::SetPosition(Vector3 newPosition)
{
    this->position = newPosition;
    this->UpdateTransform();
}

void CollidableModel::SetScale(Vector3 newScale)
{
    this->scale = newScale;
    this->UpdateScale();
}

void CollidableModel::SetRotation(Vector3 axis, float angleDeg)
{
    this->rotationAxis = axis;
    this->rotationAngleDeg = angleDeg;
    this->UpdateTransform();
}

bool CollidableModel::BuildMeshShape(Model *modelIn)
{
    this->meshInterface = std::make_unique<btTriangleIndexVertexArray>();
    this->meshBuffers.clear();
    this->meshBuffers.reserve(modelIn->meshCount);

    for (int i = 0; i < modelIn->meshCount; ++i)
    {
        const Mesh &mesh = modelIn->meshes[i];
        if (!ValidateMesh(mesh))
        {
            continue;
        }

        MeshBuffers buffers = CopyMeshData(mesh);
        if (buffers.indices.empty() || buffers.vertices.empty())
        {
            continue;
        }

        btIndexedMesh indexed;
        indexed.m_numTriangles = static_cast<int>(buffers.indices.size() / 3);
        indexed.m_triangleIndexBase = reinterpret_cast<const unsigned char *>(buffers.indices.data());
        indexed.m_triangleIndexStride = 3 * sizeof(int);
        indexed.m_numVertices = mesh.vertexCount;
        indexed.m_vertexBase = reinterpret_cast<const unsigned char *>(buffers.vertices.data());
        indexed.m_vertexStride = 3 * sizeof(btScalar);
        indexed.m_indexType = PHY_INTEGER;

        this->meshInterface->addIndexedMesh(indexed, PHY_INTEGER);
        this->meshBuffers.push_back(std::move(buffers));
    }

    if (this->meshInterface->getIndexedMeshArray().size() == 0)
    {
        return false;
    }

    this->meshShape = std::make_unique<btBvhTriangleMeshShape>(this->meshInterface.get(), true, true);
    this->meshShape->setMargin(0.01f);
    this->collisionObject = std::make_unique<btCollisionObject>();
    this->collisionObject->setCollisionShape(this->meshShape.get());
    this->collisionObject->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
    return true;
}

void CollidableModel::UpdateTransform()
{
    if (!this->collisionObject)
    {
        return;
    }

    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(this->position.x, this->position.y, this->position.z));
    btQuaternion rotation = QuaternionFromAxisAngleSafe(this->rotationAxis, this->rotationAngleDeg);
    transform.setRotation(rotation);
    this->collisionObject->setWorldTransform(transform);
}

void CollidableModel::UpdateScale()
{
    if (!this->meshShape)
    {
        return;
    }
    this->meshShape->setLocalScaling(btVector3(this->scale.x, this->scale.y, this->scale.z));
    this->meshShape->recalcLocalAabb();
}

CollidableModel::MeshBuffers CollidableModel::CopyMeshData(const Mesh &mesh)
{
    MeshBuffers buffers;
    buffers.vertices.resize(mesh.vertexCount * 3);
    for (int i = 0; i < mesh.vertexCount * 3; ++i)
    {
        buffers.vertices[i] = static_cast<btScalar>(mesh.vertices[i]);
    }

    if (mesh.indices != nullptr && mesh.triangleCount > 0)
    {
        buffers.indices.resize(mesh.triangleCount * 3);
        for (int i = 0; i < mesh.triangleCount * 3; ++i)
        {
            buffers.indices[i] = static_cast<int>(mesh.indices[i]);
        }
    }
    else
    {
        buffers.indices.resize(mesh.triangleCount * 3);
        for (int tri = 0; tri < mesh.triangleCount; ++tri)
        {
            buffers.indices[tri * 3 + 0] = tri * 3 + 0;
            buffers.indices[tri * 3 + 1] = tri * 3 + 1;
            buffers.indices[tri * 3 + 2] = tri * 3 + 2;
        }
    }
    return buffers;
}

bool CollidableModel::ValidateMesh(const Mesh &mesh)
{
    return mesh.vertexCount > 0 && mesh.triangleCount > 0 && mesh.vertices != nullptr;
}
