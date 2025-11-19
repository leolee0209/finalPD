#pragma once
#include "attack.hpp"
#include <vector>
#include "object.hpp"
class AttackManager
{
private:
    std::vector<ThousandAttack *> thousandAttack;

public:
    ~AttackManager();
    void update();
    ThousandAttack *getThousandAttack(Entity *spawnedBy);
    std::vector<const Object *> getObjects()const;
};
