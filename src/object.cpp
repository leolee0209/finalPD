#include "object.hpp"
#include "scene.hpp"
void Object::UpdateOBB()
{
    this->obb.center = this->pos;
    this->obb.halfExtents = Vector3Scale(this->size, 0.5f);
    this->obb.rotation = this->rotation;
}

CollisionResult Object::collided(Object &thiso, Object &other)
{
    other.UpdateOBB();
    return GetCollisionOBBvsOBB(&thiso.obb, &other.obb);
}

std::vector<CollisionResult> Object::collided(Object &thiso, Scene *scene)
{
    std::vector<CollisionResult> r;
    thiso.UpdateOBB();
    for (auto &o : scene->getObjects())
    {
        CollisionResult cr = Object::collided(thiso, *o);
        if (cr.collided)
            r.push_back(cr);
    }
    return r;
}
