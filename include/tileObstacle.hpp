#pragma once
#include <raylib.h>
#include <memory>

class TileObstacle {
public:
    TileObstacle(Model* model, int tileIndex, Vector3 position);
    void Draw() const;

private:
    Model* model;
    int tileIndex;
    Vector3 position;
};
