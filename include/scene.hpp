#pragma once
#include <vector>
#include <raylib.h>
#include <me.hpp>
#include "object.hpp"
#include <memory>
class Scene
{
private:
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<Object> objects;
    Object floor;
    void DrawRectangle(const Object &o) const;

public:
    void addEntity(std::unique_ptr<Entity> e);
    void DrawScene();
    void Update();
    Scene();
    bool collided(); // TODO check collision for all objects
};