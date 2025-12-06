#include "tileObject.hpp"

TileObject::TileObject(TileModelManager* manager, TileType type, Vector3 position, Quaternion rotation, float scaleFactor)
    : Object(), manager(manager), type(type), scaleFactor(scaleFactor)
{
    this->pos = position;
    this->rotation = rotation;

    // Dimensions based on 2.0m x 3.0m x 1.0m as per plan
    this->size = {2.0f * scaleFactor, 3.0f * scaleFactor, 1.0f * scaleFactor};

    this->setAsBox(this->size); // Updates OBB
    this->visible = true;
}

void TileObject::Draw() const
{
    if (this->manager)
    {
        this->manager->DrawTile(this->type, this->pos, this->rotation, {this->scaleFactor, this->scaleFactor, this->scaleFactor});
    }
}
