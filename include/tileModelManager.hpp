#pragma once
#include "raylib.h"
#include "tiles.hpp"

class TileModelManager {
public:
    TileModelManager();
    ~TileModelManager();
    void DrawTile(TileType type, Vector3 position, Quaternion rotation, Vector3 scale) const;
    void SetShader(Shader shader);

private:
    Model model;
};
