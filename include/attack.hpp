#pragma once
#include "me.hpp"
#include <vector>
#include "object.hpp"
class AttackController
{
protected:
    AttackController(Entity *_spawnedBy) : spawnedBy(_spawnedBy) {}

public:
    Entity *const spawnedBy; // Change from Me* to Entity*
    virtual void update() = 0;
};
class ThousandAttack : public AttackController
{
private:
    int count;
    std::vector<Projectile> projectiles;

public:
    ThousandAttack(Entity *_spawnedBy) : AttackController(_spawnedBy)
    {
        this->count = 0;
    }
    void spawnProjectile();
    void update() override;
    std::vector<const Object *> obj()
    {
        std::vector<const Object *> ret;
        for (const auto &p : this->projectiles)
            ret.push_back(&p.obj());
        return ret;
    }
};