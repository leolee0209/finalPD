#pragma once
#include "raylib.h"
#include "tiles.hpp"

class TileModelManager {
public:
    TileModelManager();
    ~TileModelManager();
    void DrawTile(TileType type, Vector3 position) const;

private:
    Model model;
};
