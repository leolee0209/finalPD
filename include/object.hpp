#pragma once
#include <raylib.h>

class Object // maybe first expect it to be a box(3d rectangle)
{
protected:
    Vector3 size;
    Vector3 pos;

public:
    bool collided(); // maybe change bool to "collision information" to better convey collision
    const Vector3 &getSize() const { return this->size; }
    const Vector3 &getPos() const { return this->pos; }
    Object() {};
    Object(Vector3 sizes, Vector3 poss)
    {
        this->size = sizes;
        this->pos = poss;
    };
};
