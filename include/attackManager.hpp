#pragma once
#include "attack.hpp"
#include <vector>
#include "object.hpp"

// Manages all attacks in the game, instance hold by scene
class AttackManager
{
private:
    std::vector<ThousandAttack *> thousandAttack; // List of ThousandAttack instances

public:
    ~AttackManager(); // Destructor to clean up dynamically allocated attacks

    // Updates all attacks managed by the AttackManager
    void update();

    // Retrieves or creates a ThousandAttack for the given entity
    ThousandAttack *getThousandAttack(Entity *spawnedBy);

    // Returns a list of objects representing all projectiles for rendering or collision detection
    std::vector<const Object *> getObjects() const;
};
