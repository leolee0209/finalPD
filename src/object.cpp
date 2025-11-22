#include "object.hpp"

void Object::UpdateOBB()
{
    this->obb.center = this->pos;
    this->obb.halfExtents = Vector3Scale(this->size, 0.5f);
    this->obb.rotation = this->rotation;
}

CollisionResult Object::collided(Object& other)
{
    return GetCollisionOBBvsOBB(&this->obb, &other.obb);
}
