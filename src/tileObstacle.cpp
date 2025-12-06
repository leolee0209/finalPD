#include "tileObstacle.hpp"
#include "raymath.h"
#include <rlgl.h>

TileObstacle::TileObstacle(Model* model, int tileIndex, Vector3 position)
    : model(model), tileIndex(tileIndex), position(position)
{
}

void TileObstacle::Draw() const
{
    if (this->model && this->tileIndex >= 0)
    {
        int baseMeshIndex = this->tileIndex * 3;
        if (baseMeshIndex + 2 < this->model->meshCount)
        {
            rlPushMatrix();
            rlTranslatef(this->position.x, this->position.y, this->position.z);
            DrawMesh(this->model->meshes[baseMeshIndex], this->model->materials[this->model->meshMaterial[baseMeshIndex]], MatrixIdentity());
            DrawMesh(this->model->meshes[baseMeshIndex + 1], this->model->materials[this->model->meshMaterial[baseMeshIndex + 1]], MatrixIdentity());
            DrawMesh(this->model->meshes[baseMeshIndex + 2], this->model->materials[this->model->meshMaterial[baseMeshIndex + 2]], MatrixIdentity());
            rlPopMatrix();
        }
    }
}
