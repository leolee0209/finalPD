#pragma once
#include <raylib.h>
#include <raymath.h>
#include "obb.hpp"
#include <vector>
class Entity;
class Scene;
/**
 * @brief Simple axis-aligned/rotated 3D box Object used for physics and rendering.
 *
 * `Object` represents a textured or untextured box in world space. It stores
 * `size`, `pos` and `rotation` (quaternion) and provides helpers to update the
 * cached `OBB` for collision tests. Use `useTexture` and `sourceRect` when
 * drawing textured cubes via Scene helpers.
 */
enum class ObjectShape
{
    Box,
    Sphere
};

class Object
{
public:
    Vector3 size;
    Vector3 pos;
    Quaternion rotation;
    OBB obb;
    ObjectShape shape = ObjectShape::Box;
    float sphereRadius = 0.5f;
    Texture2D *texture = nullptr;
    Rectangle sourceRect;
    bool useTexture = false;
    Color tint = WHITE; // Rendering tint for textured objects
    /**
     * @brief Whether this Object should be rendered by Scene drawing helpers.
     *
     * Some Objects are purely physical (collision helper volumes, invisible
     * effects, or helper geometry) and should not be drawn. Set this to
     * `false` to hide the object while preserving its OBB/collision data.
     */
    bool visible = true;

    /**
     * @brief Test collision between two Objects and return a CollisionResult.
     *
     * This function always returns a CollisionResult for the pair. Use the
     * returned `collided` flag to check whether penetration occurred.
     */
    static CollisionResult collided(Object &thiso, Object &other);

    /**
     * @brief Test collision between this object and all static scene objects.
     *
     * Returns a vector of CollisionResult entries (may be empty).
     */
    static std::vector<CollisionResult> collided(Object &thiso, Scene *scene);

    /**
     * @brief Recompute the object's OBB from `pos`, `size` and `rotation`.
     *
     * Call whenever `pos`, `size` or `rotation` change before running collision
     * queries.
     */
    void UpdateOBB();

    // Shape helpers
    void setAsBox(Vector3 sizes);
    void setAsSphere(float radius);
    bool isSphere() const { return this->shape == ObjectShape::Sphere; }
    float getSphereRadius() const { return this->sphereRadius; }

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

    /**
     * @brief Apply an incremental rotation around an axis.
     *
     * @param axis Rotation axis in world/local space (will be normalized).
     * @param angleDeg Angle in degrees to rotate by.
     */
    void rotate(const Vector3 &axis, float angleDeg)
    {
        Quaternion q_rot = QuaternionFromAxisAngle(Vector3Normalize(axis), angleDeg * DEG2RAD);
        this->rotation = QuaternionMultiply(q_rot, this->rotation);
    }

    void rotate(const Quaternion &q)
    {
        this->rotation = QuaternionMultiply(q, this->rotation);
    }

    /**
     * @brief Set rotation so the object's local +Z faces the provided direction.
     *
     * Useful for orienting visual models to face a movement or target
     * direction. The `forward` vector will be normalized internally.
     *
     * @param forward Target forward direction in world space.
     */
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

    /**
     * @brief Default constructor: unit cube at origin with identity rotation.
     */
    Object() : size{1, 1, 1}, pos{0, 0, 0}, rotation(QuaternionIdentity()) { setAsBox(this->size); };

    /**
     * @brief Construct with explicit size and position (identity rotation).
     */
    Object(Vector3 sizes, Vector3 poss) : size(sizes), pos(poss), rotation(QuaternionIdentity()) { setAsBox(sizes); };

    /**
     * @brief Construct with explicit size, position and rotation.
     */
    Object(Vector3 sizes, Vector3 poss, Quaternion rot) : size(sizes), pos(poss), rotation(rot) { setAsBox(sizes); };
    
    // Visibility helpers
    void setVisible(bool v) { this->visible = v; }
    bool isVisible() const { return this->visible; }
};
