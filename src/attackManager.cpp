#include "attackManager.hpp"
#include <iostream>
AttackManager::~AttackManager()
{
    for (auto &a : this->thousandAttack)
        delete a;
}

void AttackManager::update()
{
    for (auto &a : this->thousandAttack)
        a->update();
}

ThousandAttack *AttackManager::getThousandAttack(Entity *spawnedBy)
{
    for (const auto &a : this->thousandAttack)
    {
        if (a->spawnedBy == spawnedBy)
            return a;
    }
    this->thousandAttack.push_back(new ThousandAttack(spawnedBy));
    return this->thousandAttack.back();
}

std::vector<const Object *> AttackManager::getObjects() const
{
    std::vector<const Object *> ret;
    for (const auto &a : this->thousandAttack)
    {
        auto v = a->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    return ret;
}
