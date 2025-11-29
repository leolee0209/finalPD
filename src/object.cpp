#include "object.hpp"
#include "scene.hpp"
#include "me.hpp"
#include <cmath>
namespace
{
Vector3 ClosestPointOnOBB(const OBB &obb, Vector3 point)
{
    Vector3 local = Vector3Subtract(point, obb.center);
    Quaternion invRot = QuaternionInvert(obb.rotation);
    local = Vector3RotateByQuaternion(local, invRot);
    Vector3 clamped = {
        Clamp(local.x, -obb.halfExtents.x, obb.halfExtents.x),
        Clamp(local.y, -obb.halfExtents.y, obb.halfExtents.y),
        Clamp(local.z, -obb.halfExtents.z, obb.halfExtents.z)};
    Vector3 world = Vector3RotateByQuaternion(clamped, obb.rotation);
    return Vector3Add(world, obb.center);
}

CollisionResult CollideBoxBox(Object &a, Object &b)
{
    a.UpdateOBB();
    b.UpdateOBB();
    CollisionResult result = GetCollisionOBBvsOBB(&a.obb, &b.obb);
    result.with = nullptr;
    return result;
}

CollisionResult CollideSphereSphere(Object &a, Object &b)
{
    CollisionResult result{};
    result.with = nullptr;
    result.collided = false;
    Vector3 delta = Vector3Subtract(a.pos, b.pos);
    float radiusSum = a.getSphereRadius() + b.getSphereRadius();
    float distSq = Vector3LengthSqr(delta);
    if (distSq > radiusSum * radiusSum)
    {
        return result;
    }
    float dist = sqrtf(distSq);
    Vector3 normal = {0.0f, 1.0f, 0.0f};
    if (dist > 0.0001f)
    {
        normal = Vector3Scale(delta, 1.0f / dist);
    }
    result.collided = true;
    result.normal = normal;
    result.penetration = radiusSum - dist;
    if (result.penetration < 0.0f)
        result.penetration = 0.0f;
    return result;
}

CollisionResult CollideSphereBox(Object &sphereObj, Object &boxObj, bool sphereFirst)
{
    boxObj.UpdateOBB();
    CollisionResult result{};
    result.with = nullptr;
    result.collided = false;

    Vector3 closest = ClosestPointOnOBB(boxObj.obb, sphereObj.pos);
    Vector3 delta = Vector3Subtract(sphereObj.pos, closest);
    float distSq = Vector3LengthSqr(delta);
    float radius = sphereObj.getSphereRadius();
    if (distSq > radius * radius)
    {
        return result;
    }

    float dist = sqrtf(distSq);
    Vector3 normal;
    if (dist > 0.0001f)
    {
        normal = Vector3Scale(delta, 1.0f / dist);
    }
    else
    {
        Vector3 bias = Vector3Subtract(sphereObj.pos, boxObj.pos);
        if (Vector3LengthSqr(bias) < 0.0001f)
            bias = {0.0f, 1.0f, 0.0f};
        normal = Vector3Normalize(bias);
        dist = 0.0f;
    }

    if (!sphereFirst)
    {
        normal = Vector3Negate(normal);
    }

    result.collided = true;
    result.normal = normal;
    float penetration = radius - dist;
    if (penetration < 0.0f)
        penetration = 0.0f;
    result.penetration = penetration;
    return result;
}
}

void Object::UpdateOBB()
{
    this->obb.center = this->pos;
    if (this->shape == ObjectShape::Sphere)
    {
        this->obb.halfExtents = {this->sphereRadius, this->sphereRadius, this->sphereRadius};
        this->obb.rotation = QuaternionIdentity();
    }
    else
    {
        this->obb.halfExtents = Vector3Scale(this->size, 0.5f);
        this->obb.rotation = this->rotation;
    }
}

void Object::setAsBox(Vector3 sizes)
{
    this->shape = ObjectShape::Box;
    this->size = sizes;
    this->sphereRadius = fmaxf(fmaxf(sizes.x, sizes.y), sizes.z) * 0.5f;
    this->UpdateOBB();
}

void Object::setAsSphere(float radius)
{
    this->shape = ObjectShape::Sphere;
    this->sphereRadius = radius;
    this->size = {radius * 2.0f, radius * 2.0f, radius * 2.0f};
    this->UpdateOBB();
}

CollisionResult Object::collided(Object &thiso, Object &other)
{
    if (thiso.shape == ObjectShape::Box && other.shape == ObjectShape::Box)
    {
        return CollideBoxBox(thiso, other);
    }
    if (thiso.shape == ObjectShape::Sphere && other.shape == ObjectShape::Sphere)
    {
        return CollideSphereSphere(thiso, other);
    }
    if (thiso.shape == ObjectShape::Sphere && other.shape == ObjectShape::Box)
    {
        return CollideSphereBox(thiso, other, true);
    }
    if (thiso.shape == ObjectShape::Box && other.shape == ObjectShape::Sphere)
    {
        return CollideSphereBox(other, thiso, false);
    }

    // Fallback to OBB test
    thiso.UpdateOBB();
    other.UpdateOBB();
    auto result = GetCollisionOBBvsOBB(&thiso.obb, &other.obb);
    result.with = nullptr;
    return result;
}

std::vector<CollisionResult> Object::collided(Object &thiso, Scene *scene)
{
    std::vector<CollisionResult> r;
    thiso.UpdateOBB();
    for (auto &o : scene->getStaticObjects())
    {
        CollisionResult cr = Object::collided(thiso, *o);
        if (cr.collided)
            r.push_back(cr);
    }
    for (auto &e : scene->getEntities())
    {
        CollisionResult cr = Object::collided(thiso, e->obj());
        cr.with = e;
        if (cr.collided)
            r.push_back(cr);
    }
    return r;
}
