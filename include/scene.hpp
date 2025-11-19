#pragma once
#include <vector>
#include <attackManager.hpp>
#include <me.hpp>
#include "object.hpp"
class Scene
{
private:
    std::vector<Entity> entities;
    std::vector<Object> objects;
    Object floor;
    void DrawRectangle(const Object &o) const;

public:
    AttackManager am;
    ~Scene() {}
    void addEntity(Entity e);
    void DrawScene() const;
    void Update();
    Scene();
    bool collided(); // TODO check collision for all objects
};