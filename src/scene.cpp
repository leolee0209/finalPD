#include "scene.hpp"
#include "me.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <cmath>
#include <iostream>

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

bool Scene::collided(Object other)
{
    int otherX = other.getPos().x, otherY = other.getPos().y;
    for(auto tower: this->objects)
    {
        int myX = tower.getPos().x, myY = tower.getPos().y;
        int deltaX = abs(myX-otherX), deltaY = abs(myY-otherY);
        if(sqrt(deltaX*deltaX + deltaY*deltaY) <= 10)
        {
            return true;
        }
    }
    return false;
}

bool Scene::collided(Me other)
{
    int otherX = other.px(), otherY = other.py();
    for(auto tower: this->objects)
    {
        int myX = tower.getPos().x, myY = tower.getPos().y;
        int deltaX = abs(myX-otherX), deltaY = abs(myY-otherY);
        std::cout << sqrt(deltaX*deltaX + deltaY*deltaY) << std::endl;
        if(sqrt(deltaX*deltaX + deltaY*deltaY) <= 20)
        {
            return true;
        }
    }
    return false;
}