#pragma once
#include <vector>
#include "updateContext.hpp"
#include "me.hpp"
#include "object.hpp"
#include <raylib.h>
class Enemy;
class Object;
struct DamageResult;
class EnemyManager
{
private:
    std::vector<Enemy *> enemies;
public:
    ~EnemyManager();

    void addEnemy(Enemy *e);
    void RemoveEnemy(Enemy *e);
    void RemoveEnemiesInBounds(const BoundingBox &bounds);
    void update(UpdateContext &uc);
    void damage(Enemy *enemy, DamageResult &dResult, UpdateContext &uc);
    std::vector<Object *> getObjects() const;
    std::vector<Entity *> getEntities(EntityCategory cat = ENTITY_ENEMY) const;
    void clear();
};