#include "object.hpp"
#include "scene.hpp"
#include "me.hpp"
void Object::UpdateOBB()
{
    this->obb.center = this->pos;
    this->obb.halfExtents = Vector3Scale(this->size, 0.5f);
    this->obb.rotation = this->rotation;
}

CollisionResult Object::collided(Object &thiso, Object &other)
{
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
