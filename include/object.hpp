#pragma once
#include <raylib.h>

class Object // maybe first expect it to be a box(3d rectangle)
{
protected:
    Vector3 size; // Dimensions of the object (width, height, depth)
    Vector3 pos;  // Position of the object in 3D space

public:
    // Checks if the object has collided with another object
    // TODO: Replace `bool` with a more detailed "collision information" structure
    bool collided();

    // Getter for the object's size
    const Vector3 &getSize() const { return this->size; }

    // Getter for the object's position
    const Vector3 &getPos() const { return this->pos; }

    // Default constructor initializes the object with default values
    Object() {};

    // Parameterized constructor initializes the object with specific size and position
    Object(Vector3 sizes, Vector3 poss)
    {
        this->size = sizes;
        this->pos = poss;
    };
};
