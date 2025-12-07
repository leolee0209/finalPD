#include "tileObject.hpp"
#include "model_constants.hpp"

TileObject::TileObject(TileModelManager* mgr, TileType tType, Vector3 pos, Quaternion rot, float sFactor)
    : Object(), manager(mgr), type(tType), scaleFactor(sFactor)
{
    this->pos = pos;
    this->rotation = rot;

    // Dimensions based on actual model size scaled by factor
    this->size = {TILE_MODEL_SIZE.x * sFactor, TILE_MODEL_SIZE.y * sFactor, TILE_MODEL_SIZE.z * sFactor};

    this->setAsBox(this->size); // Updates OBB
    this->visible = true; // Must be true to be drawn by Scene
}

void TileObject::Draw() const
{
    if (this->manager)
    {
        // Calculate scale to fit the model into the physics box
        // Model is TILE_MODEL_SIZE, we want it to be this->size
        Vector3 visualScale = {
            this->size.x / TILE_MODEL_SIZE.x,
            this->size.y / TILE_MODEL_SIZE.y,
            this->size.z / TILE_MODEL_SIZE.z
        };
        
        this->manager->DrawTile(this->type, this->pos, this->rotation, visualScale);
    }
}
