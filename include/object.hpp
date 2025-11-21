#pragma once
#include <raylib.h>
#include <raymath.h>

class Object // maybe first expect it to be a box(3d rectangle)
{
protected:
    Vector3 size;         // Dimensions of the object (width, height, depth)
    Vector3 pos;          // Position of the object in 3D space
    Vector3 rotationAxis; // Axis to rotate around (default up)
    float rotationAngle;  // Rotation angle in degrees

public:
    // Checks if the object has collided with another object
    // TODO: Replace `bool` with a more detailed "collision information" structure
    bool collided();

    // Getter for the object's size
    const Vector3 &getSize() const { return this->size; }

    // Getter for the object's position
    const Vector3 &getPos() const { return this->pos; }

    // Getter for rotation axis and angle
    const Vector3 &getRotationAxis() const { return this->rotationAxis; }
    float getRotationAngle() const { return this->rotationAngle; }

    // Set rotation (axis should be normalized if non-default)
    void setRotation(const Vector3 &axis, float angleDeg)
    {
        this->rotationAxis = axis;
        this->rotationAngle = angleDeg;
    }

    // Set rotation so the object's forward (+Z) faces `forward` direction.
    // Computes rotation axis and angle and stores them (angle in degrees).
    void setRotationFromForward(Vector3 forward)
    {
        Vector3 modelForward = {0.0f, 0.0f, 1.0f};
        Vector3 fNorm = Vector3Normalize(forward);
        float angleRad = Vector3Angle(modelForward, fNorm); // radians
        Vector3 rotAxis = Vector3CrossProduct(modelForward, fNorm);
        if (Vector3Length(rotAxis) < 1e-6f)
        {
            // Fallback axis when vectors are parallel or anti-parallel
            rotAxis = {0.0f, 1.0f, 0.0f};
        }
        rotAxis = Vector3Normalize(rotAxis);
        this->rotationAxis = rotAxis;
        this->rotationAngle = angleRad * RAD2DEG; // convert to degrees
    }

    // Default constructor initializes the object with default values
    Object()
    {
        this->rotationAxis = {0.0f, 1.0f, 0.0f};
        this->rotationAngle = 0.0f;
    };

    // Parameterized constructor initializes the object with specific size and position
    Object(Vector3 sizes, Vector3 poss)
    {
        this->size = sizes;
        this->pos = poss;
        this->rotationAxis = {0.0f, 1.0f, 0.0f};
        this->rotationAngle = 0.0f;
    };

    // Parameterized constructor with explicit rotation
    Object(Vector3 sizes, Vector3 poss, Vector3 rotAxis, float rotAngle)
    {
        this->size = sizes;
        this->pos = poss;
        this->rotationAxis = rotAxis; // explicit axis
        this->rotationAngle = rotAngle;
    };
};
