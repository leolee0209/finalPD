#pragma once
#include "tileObject.hpp"

class WallTile : public TileObject {
public:
    WallTile(TileModelManager* manager, TileType type, Vector3 position, Quaternion rotation, float scaleFactor);
};
