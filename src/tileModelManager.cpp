#include "tileModelManager.hpp"
#include "raymath.h"
#include <rlgl.h>

TileModelManager::TileModelManager()
{
    this->model = LoadModel("mahjong/scene.gltf");
}

TileModelManager::~TileModelManager()
{
    UnloadModel(this->model);
}

void TileModelManager::DrawTile(TileType type, Vector3 position) const
{
    int tileIndex = (int)type;
    int baseMeshIndex = tileIndex * 3;

    if (baseMeshIndex + 2 < this->model.meshCount)
    {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        DrawMesh(this->model.meshes[baseMeshIndex], this->model.materials[this->model.meshMaterial[baseMeshIndex]], MatrixIdentity());
        DrawMesh(this->model.meshes[baseMeshIndex + 1], this->model.materials[this->model.meshMaterial[baseMeshIndex + 1]], MatrixIdentity());
        DrawMesh(this->model.meshes[baseMeshIndex + 2], this->model.materials[this->model.meshMaterial[baseMeshIndex + 2]], MatrixIdentity());
        rlPopMatrix();
    }
}
