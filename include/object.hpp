#pragma once
#include <raylib.h>
#include <raymath.h>
#include "obb.hpp"
#include <vector>

class Scene;
class Object // maybe first expect it to be a box(3d rectangle)
{
public:
    Vector3 size;
    Vector3 pos;
    Quaternion rotation;
    OBB obb;
    Texture2D* texture = nullptr;
    Rectangle sourceRect;
    bool useTexture = false;

    // Checks if the object has collided with another object
    static CollisionResult collided(Object& thiso, Object& other);
    static std::vector<CollisionResult> collided(Object &thiso, Scene* scene);

    void UpdateOBB();

    // Getter for the object's size
    const Vector3 &getSize() const { return this->size; }

    // Getter for the object's position
    const Vector3 &getPos() const { return this->pos; }

    // Getter for rotation as a quaternion
    const Quaternion &getRotation() const { return this->rotation; }

    // Returns rotation as axis and angle (in degrees) for drawing
    void getRotationAxisAngle(Vector3 &axis, float &angle) const
    {
        QuaternionToAxisAngle(this->rotation, &axis, &angle);
        angle *= RAD2DEG;
    }

    // Sets the rotation to a new orientation
    void setRotation(const Quaternion &q)
    {
        this->rotation = q;
    }

    // Applies a rotation on top of the current rotation
    void rotate(const Vector3 &axis, float angleDeg)
    {
        Quaternion q_rot = QuaternionFromAxisAngle(Vector3Normalize(axis), angleDeg * DEG2RAD);
        this->rotation = QuaternionMultiply(q_rot, this->rotation);
    }
    
    void rotate(const Quaternion &q)
    {
        this->rotation = QuaternionMultiply(q, this->rotation);
    }

    // Set rotation so the object's forward (+Z) faces `forward` direction.
    // Computes rotation axis and angle and stores them (angle in degrees).
    void setRotationFromForward(Vector3 forward)
    {
        Vector3 modelForward = {0.0f, 0.0f, 1.0f};
        Vector3 fNorm = Vector3Normalize(forward);
        float angleRad = Vector3Angle(modelForward, fNorm);
        Vector3 rotAxis = Vector3CrossProduct(modelForward, fNorm);
        if (Vector3Length(rotAxis) < 1e-6f)
        {
            // Fallback axis when vectors are parallel or anti-parallel
            rotAxis = {0.0f, 1.0f, 0.0f};
        }
        this->rotation = QuaternionFromAxisAngle(Vector3Normalize(rotAxis), angleRad);
    }

    // Default constructor initializes the object with default values
    Object() : size{1,1,1}, pos{0,0,0}, rotation(QuaternionIdentity()) {};

    // Parameterized constructor initializes the object with specific size and position
    Object(Vector3 sizes, Vector3 poss) : size(sizes), pos(poss), rotation(QuaternionIdentity()) {};

    // Parameterized constructor with explicit rotation
    Object(Vector3 sizes, Vector3 poss, Quaternion rot) : size(sizes), pos(poss), rotation(rot) {};
};
