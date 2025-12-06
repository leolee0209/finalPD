#pragma once
#include "object.hpp"
#include "tileModelManager.hpp"

class TileObject : public Object {
public:
    TileObject(TileModelManager* manager, TileType type, Vector3 position, Quaternion rotation, float scaleFactor);
    void Draw() const;

private:
    TileModelManager* manager;
    TileType type;
    float scaleFactor;
};
