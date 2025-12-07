#pragma once
#include "raylib.h"
#include "tiles.hpp"
#include <vector>

class TileModelManager {
public:
    TileModelManager();
    ~TileModelManager();
    void DrawTile(TileType type, Vector3 position, Quaternion rotation, Vector3 scale) const;
    void DrawTileInstanced(TileType type, const std::vector<Matrix>& transforms) const;
    void SetShader(Shader shader);
    Shader GetShader() const { return model.materials[0].shader; }
    BoundingBox GetModelBoundingBox() const;
    BoundingBox GetTileBoundingBox(TileType type) const;
    Vector3 GetTileCenter(TileType type) const;

private:
    Model model;
    std::vector<Vector3> tileCenters;
};
