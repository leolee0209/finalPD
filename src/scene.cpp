#include "scene.hpp"
#include <raylib.h>
#include "constant.hpp"

void Scene::DrawRectangle(Object &o)
{
    DrawCubeV(o.getPos(), o.getSize(), TOWER_COLOR);
    DrawCubeWiresV(o.getPos(), o.getSize(), DARKBLUE);
}

void Scene::DrawScene()
{
    const int floorExtent = 25;
    const float tileSize = 5.0f;
    const Color tileColor1 = {150, 200, 200, 255};

    // Floor tiles
    for (int y = -floorExtent; y < floorExtent; y++)
    {
        for (int x = -floorExtent; x < floorExtent; x++)
        {
            if ((y & 1) && (x & 1))
            {
                DrawPlane({x * tileSize, 0.0f, y * tileSize}, {tileSize, tileSize}, tileColor1);
            }
            else if (!(y & 1) && !(x & 1))
            {
                DrawPlane({x * tileSize, 0.0f, y * tileSize}, {tileSize, tileSize}, LIGHTGRAY);
            }
        }
    }

    for (auto &o : this->objects)
    {
        DrawRectangle(o);
    }
    // const Vector3 towerSize = {16.0f, 32.0f, 16.0f};
    // const Color towerColor = {150, 200, 200, 255};

    // Vector3 towerPos = {16.0f, 16.0f, 16.0f};
    // DrawCubeV(towerPos, towerSize, towerColor);
    // DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

    // towerPos.x *= -1;
    // DrawCubeV(towerPos, towerSize, towerColor);
    // DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

    // towerPos.z *= -1;
    // DrawCubeV(towerPos, towerSize, towerColor);
    // DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

    // towerPos.x *= -1;
    // DrawCubeV(towerPos, towerSize, towerColor);
    // DrawCubeWiresV(towerPos, towerSize, DARKBLUE);

    // Red sun
    DrawSphere({300.0f, 300.0f, 0.0f}, 100.0f, {255, 0, 0, 255});
}

Scene::Scene()
{
    const Vector3 towerSize = {16.0f, 32.0f, 16.0f};
    const Color towerColor = {150, 200, 200, 255};

    Vector3 towerPos = {16.0f, 16.0f, 16.0f};

    this->objects.push_back(Object(towerSize, towerPos));
    towerPos.x *= -1;
    this->objects.push_back(Object(towerSize, towerPos));
    towerPos.z *= -1;
    this->objects.push_back(Object(towerSize, towerPos));
    towerPos.x *= -1;
    this->objects.push_back(Object(towerSize, towerPos));
}
