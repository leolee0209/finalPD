#include "tileModelManager.hpp"
#include "raymath.h"
#include <rlgl.h>

TileModelManager::TileModelManager()
{
    this->model = LoadModel("mahjong/scene.gltf");
    
    // Pre-calculate centers for all tile types
    this->tileCenters.resize((int)TileType::TILE_COUNT);
    for (int i = 0; i < (int)TileType::TILE_COUNT; i++)
    {
        BoundingBox bb = GetTileBoundingBox((TileType)i);
        Vector3 center = Vector3Scale(Vector3Add(bb.min, bb.max), 0.5f);
        Vector3 size = Vector3Subtract(bb.max, bb.min);
        this->tileCenters[i] = center;
    }
}

TileModelManager::~TileModelManager()
{
    UnloadModel(this->model);
}

void TileModelManager::SetShader(Shader shader)
{
    for (int i = 0; i < this->model.materialCount; i++)
    {
        this->model.materials[i].shader = shader;
    }
}

BoundingBox TileModelManager::GetModelBoundingBox() const
{
    return ::GetModelBoundingBox(this->model);
}

BoundingBox TileModelManager::GetTileBoundingBox(TileType type) const
{
    int tileIndex = (int)type;
    int baseMeshIndex = tileIndex * 3;
    
    if (baseMeshIndex + 2 >= this->model.meshCount) {
        return { {0,0,0}, {0,0,0} };
    }

    // Calculate bounding box for the 3 meshes of this tile
    BoundingBox box = GetMeshBoundingBox(this->model.meshes[baseMeshIndex]);
    
    // Combine with other 2 meshes
    BoundingBox box2 = GetMeshBoundingBox(this->model.meshes[baseMeshIndex + 1]);
    BoundingBox box3 = GetMeshBoundingBox(this->model.meshes[baseMeshIndex + 2]);
    
    // Union of boxes
    box.min = Vector3Min(box.min, box2.min);
    box.min = Vector3Min(box.min, box3.min);
    box.max = Vector3Max(box.max, box2.max);
    box.max = Vector3Max(box.max, box3.max);
    
    return box;
}

void TileModelManager::DrawTile(TileType type, Vector3 position, Quaternion rotation, Vector3 scale) const
{
    int tileIndex = (int)type;
    int baseMeshIndex = tileIndex * 3;

    if (baseMeshIndex + 2 < this->model.meshCount)
    {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        Vector3 axis;
        float angle;
        QuaternionToAxisAngle(rotation, &axis, &angle);
        rlRotatef(angle * RAD2DEG, axis.x, axis.y, axis.z);

        rlScalef(scale.x, scale.y, scale.z);
        
        // Center the model
        Vector3 center = this->tileCenters[tileIndex];
        rlTranslatef(-center.x, -center.y, -center.z);

        DrawMesh(this->model.meshes[baseMeshIndex], this->model.materials[this->model.meshMaterial[baseMeshIndex]], MatrixIdentity());
        DrawMesh(this->model.meshes[baseMeshIndex + 1], this->model.materials[this->model.meshMaterial[baseMeshIndex + 1]], MatrixIdentity());
        DrawMesh(this->model.meshes[baseMeshIndex + 2], this->model.materials[this->model.meshMaterial[baseMeshIndex + 2]], MatrixIdentity());
        rlPopMatrix();
    }
}

void TileModelManager::DrawTileInstanced(TileType type, const std::vector<Matrix>& transforms) const
{
    if (transforms.empty()) return;

    int tileIndex = (int)type;
    int baseMeshIndex = tileIndex * 3;

    if (baseMeshIndex + 2 < this->model.meshCount)
    {
        // Fallback to loop drawing because the current shader (lighting.vs) 
        // does not support hardware instancing (missing instanceTransform attribute).
        for (const auto& mat : transforms) {
            for (int i = 0; i < 3; i++) {
                DrawMesh(
                    this->model.meshes[baseMeshIndex + i],
                    this->model.materials[this->model.meshMaterial[baseMeshIndex + i]],
                    mat
                );
            }
        }
    }
}

Vector3 TileModelManager::GetTileCenter(TileType type) const
{
    if ((int)type >= 0 && (int)type < (int)this->tileCenters.size()) {
        return this->tileCenters[(int)type];
    }
    return {0,0,0};
}
