#pragma once
#include <vector>
#include <raylib.h>
// TODO maybe holds information for a single thing(pos, mesh...)
class Object // maybe first expect it to be a box(3d rectangle)
{
private:
    Vector3 size;
    Vector3 pos;

public:
    bool collided(); // maybe change bool to "collision information" to better convey collision
    const Vector3 &getSize() { return this->size; }
    const Vector3 &getPos() { return this->pos; }
    Object() {};
    Object(Vector3 sizes, Vector3 poss)
    {
        this->size = sizes;
        this->pos = poss;
    };
};
class Scene
{
private:
    std::vector<Object> objects;
    Object floor;
    void DrawRectangle(Object &o);

public:
    void DrawScene();
    Scene();
    bool collided(); // TODO check collision for all objects
};