#include "attackManager.hpp"
#include <iostream>

// Destructor cleans up dynamically allocated ThousandAttack instances
AttackManager::~AttackManager()
{
    for (auto &a : this->thousandAttack)
        delete a;
    for (auto &a : this->tripletAttack)
        delete a;
}

// Updates all ThousandAttack instances
void AttackManager::update()
{
    for (auto &a : this->thousandAttack)
        a->update();
    for (auto &a : this->tripletAttack)
        a->update();
}

// Retrieves an existing ThousandAttack for the given entity or creates a new one
ThousandAttack *AttackManager::getThousandAttack(Entity *spawnedBy)
{
    for (const auto &a : this->thousandAttack)
    {
        if (a->spawnedBy == spawnedBy)
            return a;
    }

    // Create a new ThousandAttack if none exists for the entity
    this->thousandAttack.push_back(new ThousandAttack(spawnedBy));
    return this->thousandAttack.back();
}

TripletAttack *AttackManager::getTripletAttack(Entity *spawnedBy)
{
    for (const auto &a : this->tripletAttack)
    {
        if (a->spawnedBy == spawnedBy)
            return a;
    }
    // Create a new ThousandAttack if none exists for the entity
    this->tripletAttack.push_back(new TripletAttack(spawnedBy));
    return this->tripletAttack.back();
}

// Returns a list of objects representing all projectiles for rendering or collision detection
std::vector<const Object *> AttackManager::getObjects() const
{
    std::vector<const Object *> ret;

    // Collect objects from all ThousandAttack instances
    for (const auto &a : this->thousandAttack)
    {
        auto v = a->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    for (const auto &a : this->tripletAttack)
    {
        auto v = a->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    return ret;
}
