#pragma once
#include "raylib.h"
#include "raymath.h"
class Entity;
// Represents an Oriented Bounding Box (OBB) in 3D space.
// An OBB is a rectangular box that is not necessarily aligned with the coordinate axes.
// It is defined by its center position, its rotation, and its half-extents (half the size along each of its local axes).
struct OBB
{
    Quaternion rotation; // The rotation of the box.
    Vector3 center;      // The center of the box in world space.
    Vector3 halfExtents; // The half-lengths of the box along its local x, y, and z axes.
};

// Extracts the local axes (right, up, forward) of an OBB from its rotation quaternion.
// These axes are orthonormal (mutually perpendicular and of unit length).
inline void OBB_GetAxes(const OBB *obb, Vector3 *right, Vector3 *up, Vector3 *forward)
{
    // Convert the quaternion to a 3x3 rotation matrix.
    // The columns of the rotation matrix are the local axes of the rotated object.
    Matrix rot = QuaternionToMatrix(obb->rotation);

    *right = (Vector3){rot.m0, rot.m1, rot.m2};    // First column
    *up = (Vector3){rot.m4, rot.m5, rot.m6};       // Second column
    *forward = (Vector3){rot.m8, rot.m9, rot.m10}; // Third column
}

// Calculates the 8 corners of the OBB in world space.
inline void OBB_GetCorners(const OBB *obb, Vector3 corners[8])
{
    Vector3 right, up, forward;
    OBB_GetAxes(obb, &right, &up, &forward);

    // Scale the axes by the half-extents to get the vectors from the center to the faces of the box.
    right = Vector3Scale(right, obb->halfExtents.x);
    up = Vector3Scale(up, obb->halfExtents.y);
    forward = Vector3Scale(forward, obb->halfExtents.z);

    // Calculate the 8 corners by adding/subtracting the scaled axes from the center point.
    // This is done by taking all combinations of ±right, ±up, ±forward.
    corners[0] = Vector3Add(Vector3Add(Vector3Add(obb->center, right), up), forward);
    corners[1] = Vector3Add(Vector3Add(Vector3Subtract(obb->center, right), up), forward);
    corners[2] = Vector3Add(Vector3Add(Vector3Subtract(obb->center, right), up), Vector3Negate(forward));
    corners[3] = Vector3Add(Vector3Add(Vector3Add(obb->center, right), up), Vector3Negate(forward));

    corners[4] = Vector3Add(Vector3Add(Vector3Add(obb->center, right), Vector3Negate(up)), forward);
    corners[5] = Vector3Add(Vector3Add(Vector3Subtract(obb->center, right), Vector3Negate(up)), forward);
    corners[6] = Vector3Add(Vector3Add(Vector3Subtract(obb->center, right), Vector3Negate(up)), Vector3Negate(forward));
    corners[7] = Vector3Add(Vector3Add(Vector3Add(obb->center, right), Vector3Negate(up)), Vector3Negate(forward));
}

// Draws the wireframe of an OBB using 3D lines.
inline void OBB_DrawWireframe(const OBB *obb, Color color)
{
    Vector3 c[8];
    OBB_GetCorners(obb, c);

    // Draw the 12 edges of the box by connecting the 8 corners.
    DrawLine3D(c[0], c[1], color);
    DrawLine3D(c[1], c[2], color);
    DrawLine3D(c[2], c[3], color);
    DrawLine3D(c[3], c[0], color);

    DrawLine3D(c[4], c[5], color);
    DrawLine3D(c[5], c[6], color);
    DrawLine3D(c[6], c[7], color);
    DrawLine3D(c[7], c[4], color);

    DrawLine3D(c[0], c[4], color);
    DrawLine3D(c[1], c[5], color);
    DrawLine3D(c[2], c[6], color);
    DrawLine3D(c[3], c[7], color);
}

// Checks if a point is inside an OBB.
inline bool OBB_ContainsPoint(const OBB *obb, Vector3 point)
{
    // To check for containment, we transform the point from world space to the OBB's local space.
    // In local space, the OBB is an axis-aligned box centered at the origin.
    Vector3 local = Vector3Subtract(point, obb->center); // Translate point so OBB is at origin

    // Rotate the point by the inverse of the OBB's rotation.
    Quaternion inverseRot = QuaternionInvert(obb->rotation);
    local = Vector3RotateByQuaternion(local, inverseRot);

    // Now, check if the local point is within the axis-aligned half-extents.
    return fabsf(local.x) <= obb->halfExtents.x &&
           fabsf(local.y) <= obb->halfExtents.y &&
           fabsf(local.z) <= obb->halfExtents.z;
}

// Projects an Axis-Aligned Bounding Box (AABB) onto an axis and returns the min/max interval.
// This is done by projecting all 8 corners of the box onto the axis and finding the minimum and maximum projection values.
inline void ProjectBoundingBoxOntoAxis(const BoundingBox *box, Vector3 axis, float *outMin, float *outMax)
{
    Vector3 corners[8] = {
        {box->min.x, box->min.y, box->min.z},
        {box->max.x, box->min.y, box->min.z},
        {box->max.x, box->max.y, box->min.z},
        {box->min.x, box->max.y, box->min.z},
        {box->min.x, box->min.y, box->max.z},
        {box->max.x, box->min.y, box->max.z},
        {box->max.x, box->max.y, box->max.z},
        {box->min.x, box->max.y, box->max.z}};

    // The dot product gives the length of the projection of a vector onto a unit vector.
    float min = Vector3DotProduct(corners[0], axis);
    float max = min;

    for (int i = 1; i < 8; ++i)
    {
        float projection = Vector3DotProduct(corners[i], axis);
        if (projection < min)
            min = projection;
        if (projection > max)
            max = projection;
    }

    *outMin = min;
    *outMax = max;
}

// Projects an OBB onto an axis and returns the min/max interval.
// This is a more efficient way than projecting all 8 corners.
inline void ProjectOBBOntoAxis(const OBB *obb, Vector3 axis, float *outMin, float *outMax)
{
    Vector3 right, up, forward;
    OBB_GetAxes(obb, &right, &up, &forward);

    // Project the OBB's center onto the axis to find the center of the projected interval.
    float centerProj = Vector3DotProduct(obb->center, axis);

    // The "radius" of the OBB's projection is the sum of the projections of its half-extents onto the axis.
    // We use the absolute value of the dot products because the axis might be pointing in any direction.
    float r =
        fabsf(Vector3DotProduct(right, axis)) * obb->halfExtents.x +
        fabsf(Vector3DotProduct(up, axis)) * obb->halfExtents.y +
        fabsf(Vector3DotProduct(forward, axis)) * obb->halfExtents.z;

    // The min and max of the interval are the center projection plus/minus the radius.
    *outMin = centerProj - r;
    *outMax = centerProj + r;
}

// Implements the Separating Axis Theorem (SAT) to check for collision between an AABB and an OBB.
inline bool CheckCollisionBoundingBoxVsOBB(const BoundingBox *box, const OBB *obb)
{
    // The SAT states that two convex objects do not overlap if there exists a separating axis
    // (a line) onto which the projections of the two objects do not overlap.

    // For AABB vs OBB collision, we need to test 15 potential separating axes:
    // 3 axes from the AABB (world X, Y, Z)
    // 3 axes from the OBB
    // 9 axes from the cross products of the AABB's axes and the OBB's axes.

    Vector3 aabbAxes[3] = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1}};

    Vector3 obbAxes[3];
    OBB_GetAxes(obb, &obbAxes[0], &obbAxes[1], &obbAxes[2]);

    Vector3 testAxes[15];
    int axisCount = 0;

    // Add AABB's axes
    for (int i = 0; i < 3; i++)
        testAxes[axisCount++] = aabbAxes[i];

    // Add OBB's axes
    for (int i = 0; i < 3; i++)
        testAxes[axisCount++] = obbAxes[i];

    // Add cross product axes
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            Vector3 cross = Vector3CrossProduct(aabbAxes[i], obbAxes[j]);
            // Only add the axis if it's not a zero vector (i.e., if the original axes were not parallel).
            if (Vector3LengthSqr(cross) > 0.000001f)
            {
                testAxes[axisCount++] = Vector3Normalize(cross);
            }
        }
    }

    // Test each axis
    for (int i = 0; i < axisCount; ++i)
    {
        Vector3 axis = testAxes[i];
        float minA, maxA, minB, maxB;

        // Project both boxes onto the current axis.
        ProjectBoundingBoxOntoAxis(box, axis, &minA, &maxA);
        ProjectOBBOntoAxis(obb, axis, &minB, &maxB);

        // If the projections do not overlap, we found a separating axis. No collision.
        if (maxA < minB || maxB < minA)
        {
            return false;
        }
    }

    // If no separating axis was found after testing all 15 axes, the objects are colliding.
    return true;
}

// Forward declaration for CheckCollisionOBBvsOBB
inline bool CheckCollisionOBBvsOBB(const OBB *a, const OBB *b);

// Holds the result of a collision check, including penetration depth and normal.
// This is often called the Minimum Translation Vector (MTV).
typedef struct CollisionResult
{
    Entity *with;
    bool collided;     // Is there a collision?
    float penetration; // How much are the objects overlapping?
    Vector3 normal;    // In what direction should object 'a' be pushed to resolve the collision?
} CollisionResult;

// Implements SAT to get detailed collision information between two OBBs.
inline CollisionResult GetCollisionOBBvsOBB(const OBB *a, const OBB *b)
{
    CollisionResult result = {nullptr, true, INFINITY, {0, 0, 0}};

    Vector3 axesA[3], axesB[3];
    OBB_GetAxes(a, &axesA[0], &axesA[1], &axesA[2]);
    OBB_GetAxes(b, &axesB[0], &axesB[1], &axesB[2]);

    // As with AABB vs OBB, we test 15 axes (3 from a, 3 from b, 9 from cross products).
    Vector3 testAxes[15];
    int axisCount = 0;

    for (int i = 0; i < 3; ++i)
        testAxes[axisCount++] = axesA[i];
    for (int i = 0; i < 3; ++i)
        testAxes[axisCount++] = axesB[i];

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            Vector3 cross = Vector3CrossProduct(axesA[i], axesB[j]);
            if (Vector3LengthSqr(cross) > 0.0001f)
            {
                testAxes[axisCount++] = Vector3Normalize(cross);
            }
        }
    }

    // Loop through all potential separating axes.
    for (int i = 0; i < axisCount; ++i)
    {
        Vector3 axis = testAxes[i];

        float minA, maxA, minB, maxB;
        ProjectOBBOntoAxis(a, axis, &minA, &maxA);
        ProjectOBBOntoAxis(b, axis, &minB, &maxB);

        // Check for separation on this axis. If found, there is no collision.
        if (maxA < minB || maxB < minA)
        {
            result.collided = false;
            return result;
        }

        // If they overlap, calculate the amount of overlap.
        // We are looking for the smallest overlap, as this will be the penetration depth
        // along the axis of minimum penetration (the collision normal).
        float overlap = fminf(maxA, maxB) - fmaxf(minA, minB);
        if (overlap < result.penetration)
        {
            result.penetration = overlap;
            result.normal = axis;
        }
    }

    // After checking all axes, if we're here, a collision occurred.
    // 'result.penetration' is the smallest overlap we found (the MTV magnitude).
    // 'result.normal' is the axis associated with that smallest overlap.

    // We need to ensure the normal points from object 'b' to object 'a'.
    // This means the normal should generally point opposite to the vector from a's center to b's center.
    Vector3 toCenter = Vector3Subtract(b->center, a->center);
    if (Vector3DotProduct(toCenter, result.normal) > 0)
    {
        // If the dot product is positive, the normal is pointing from A to B.
        // We want it to point from B to A to serve as a push-out vector for A.
        result.normal = Vector3Negate(result.normal);
    }

    return result;
}

// Performs a ray-OBB intersection test.
inline RayCollision GetRayCollisionOBB(Ray ray, const OBB *obb)
{
    RayCollision result = {0};
    result.hit = false;

    // The classic way to solve ray-OBB intersection is to transform the ray
    // into the OBB's local space, where the OBB becomes an axis-aligned box at the origin.
    // Then we can perform a much simpler ray-AABB intersection test.

    // Move ray into OBB's local space
    Vector3 localOrigin = Vector3Subtract(ray.position, obb->center);
    Quaternion inverseRot = QuaternionInvert(obb->rotation);
    Vector3 localRayOrigin = Vector3RotateByQuaternion(localOrigin, inverseRot);
    Vector3 localRayDir = Vector3RotateByQuaternion(ray.direction, inverseRot);

    Vector3 boxMin = Vector3Negate(obb->halfExtents);
    Vector3 boxMax = obb->halfExtents;

    // Kay/Kajiya slab method for ray-AABB intersection
    float tmin = -INFINITY;
    float tmax = INFINITY;
    Vector3 normal = {0};

    // For each axis (X, Y, Z)
    for (int i = 0; i < 3; ++i)
    {
        float origin = (&localRayOrigin.x)[i];
        float dir = (&localRayDir.x)[i];
        float min = (&boxMin.x)[i];
        float max = (&boxMax.x)[i];

        if (fabsf(dir) < 0.0001f) // Ray is parallel to the slab.
        {
            if (origin < min || origin > max)
                return result; // If outside the slab, no hit.
        }
        else
        {
            // Calculate intersection distances with the two slab planes.
            float ood = 1.0f / dir;
            float t1 = (min - origin) * ood;
            float t2 = (max - origin) * ood;
            int axis = i;

            if (t1 > t2)
            {
                float temp = t1;
                t1 = t2;
                t2 = temp;
                axis = -axis;
            } // Ensure t1 is the smaller distance

            // Update the overall intersection interval [tmin, tmax]
            if (t1 > tmin)
            {
                tmin = t1;
                // Store the normal of the slab plane that caused the 'near' intersection.
                normal = (Vector3){0};
                (&normal.x)[abs(axis)] = axis >= 0 ? -1.0f : 1.0f;
            }
            if (t2 < tmax)
                tmax = t2;

            // If the interval is invalid, the ray misses the box.
            if (tmin > tmax)
                return result;
        }
    }

    // If we get here, the ray hits the box. tmin is the intersection distance.
    result.hit = true;
    result.distance = tmin;

    // Transform the intersection point and normal from local space back to world space.
    result.point = Vector3Add(ray.position, Vector3Scale(ray.direction, tmin));
    result.normal = Vector3RotateByQuaternion(normal, obb->rotation);

    return result;
}

// Checks for collision between a sphere and an OBB.
inline bool CheckCollisionSphereVsOBB(Vector3 sphereCenter, float radius, const OBB *obb)
{
    // The strategy is to find the point on the OBB that is closest to the sphere's center.
    // If the distance between this closest point and the sphere's center is less than the sphere's radius, they are colliding.

    // Transform the sphere's center into the OBB's local space.
    Vector3 localCenter = Vector3Subtract(sphereCenter, obb->center);
    Quaternion invRot = QuaternionInvert(obb->rotation);
    localCenter = Vector3RotateByQuaternion(localCenter, invRot);

    // In local space, the OBB is an AABB centered at the origin.
    // We can find the closest point by clamping the sphere's local center to the bounds of this AABB.
    Vector3 clamped = {
        Clamp(localCenter.x, -obb->halfExtents.x, obb->halfExtents.x),
        Clamp(localCenter.y, -obb->halfExtents.y, obb->halfExtents.y),
        Clamp(localCenter.z, -obb->halfExtents.z, obb->halfExtents.z)};

    // The 'clamped' point is the closest point on the local-space AABB to the local-space sphere center.
    // We don't need to transform it back to world space. We can check the distance in local space.
    // (Note: The original code transforms it back, which also works but is less efficient).

    Vector3 worldClamped = Vector3RotateByQuaternion(clamped, obb->rotation);
    worldClamped = Vector3Add(worldClamped, obb->center);

    // Check if the squared distance is less than the squared radius.
    // Using squared distance avoids a costly square root operation.
    float distSq = Vector3DistanceSqr(sphereCenter, worldClamped);
    return distSq <= radius * radius;
}
