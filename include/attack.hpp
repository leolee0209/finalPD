#pragma once
#include "me.hpp"
#include <vector>
#include "object.hpp"
#include <raylib.h>
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
    static constexpr float finalVel = 30;
    static constexpr int activate = 3;
    Vector3 dest;
    int count;
    bool activated;
    std::vector<Projectile> projectiles;
    void activateFinal();
    bool endFinal();

public:
    ThousandAttack(Entity *_spawnedBy) : AttackController(_spawnedBy)
    {
        this->count = 0;
        this->activated = false;
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