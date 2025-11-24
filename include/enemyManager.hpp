#pragma once
#include <vector>
#include "updateContext.hpp"
#include "object.hpp"
class Enemy;
class Object;
struct DamageResult;
class EnemyManager
{
private:
    std::vector<Enemy *> enemies;
    void RemoveEnemy(Enemy *e);
public:
    ~EnemyManager();

    void addEnemy(Enemy *e);
    void update(UpdateContext &uc);
    void damage(Enemy *enemy, DamageResult &dResult);
    std::vector<Object *> getObjects() const;
    std::vector<Entity *> getEntities();
};