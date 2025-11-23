#include "attackManager.hpp"
#include <iostream>

// Destructor cleans up dynamically allocated ThousandAttack instances
AttackManager::~AttackManager()
{
    for (auto &a : this->thousandAttack)
        delete a;
    for (auto &a : this->tripletAttack)
        delete a;
    for (auto &a : this->singleTileAttack)
        delete a;
}

// Updates all ThousandAttack instances
void AttackManager::update(UpdateContext& uc)
{
    for (auto &a : this->thousandAttack)
        a->update(uc);
    for (auto &a : this->tripletAttack)
        a->update(uc);
    for (auto &a : this->singleTileAttack)
        a->update(uc);
}

void AttackManager::recordThrow(MahjongTileType tile, Entity* player, Texture2D* texture, Rectangle tile_rect)
{
    // Always spawn a single projectile for every throw
    SingleTileAttack *singleAttack = getSingleTileAttack(player);
    singleAttack->spawnProjectile(tile, texture, tile_rect);

    // Record the throw and check if it completes a special combo
    this->thrownTiles.push_back({tile, tile_rect});
    this->checkActivation(player);
}

void AttackManager::checkActivation(Entity *player)
{
    if (thrownTiles.size() < 3)
        return;

    // Check last 3 tiles
    auto& [t1, r1] = thrownTiles[thrownTiles.size() - 3];
    auto& [t2, r2] = thrownTiles[thrownTiles.size() - 2];
    auto& [t3, r3] = thrownTiles[thrownTiles.size() - 1];

    int t1_val = static_cast<int>(t1);
    int t2_val = static_cast<int>(t2);
    int t3_val = static_cast<int>(t3);

    int char_1 = static_cast<int>(MahjongTileType::CHARACTER_1);
    int char_9 = static_cast<int>(MahjongTileType::CHARACTER_9);

    bool combo_found = false;

    // Thousand attack: 3 same character tiles
    if (t1_val >= char_1 && t1_val <= char_9 && t1 == t2 && t2 == t3)
    {
        ThousandAttack *attack = getThousandAttack(player);
        SingleTileAttack *singleAttack = getSingleTileAttack(player);
        
        // Transfer projectiles from single attack to thousand attack
        attack->addProjectiles(singleAttack->takeLastProjectiles(3));
        combo_found = true;
        std::cout << "found\n";
    }
    // Triplet attack: 3 different character tiles in order
    else if (t1_val >= char_1 && t1_val <= char_9 &&
        t2_val >= char_1 && t2_val <= char_9 &&
        t3_val >= char_1 && t3_val <= char_9)
    {
        if (t1_val + 1 == t2_val && t2_val + 1 == t3_val)
        {
            TripletAttack *attack = getTripletAttack(player);
            SingleTileAttack *singleAttack = getSingleTileAttack(player);

            // Transfer projectiles from single attack to triplet attack
            attack->addProjectiles(singleAttack->takeLastProjectiles(3));
            combo_found = true;
        }
    }

    if (combo_found)
    {
        thrownTiles.clear();
    }
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

SingleTileAttack *AttackManager::getSingleTileAttack(Entity *spawnedBy)
{
    for (const auto &a : this->singleTileAttack)
    {
        if (a->spawnedBy == spawnedBy)
            return a;
    }
    // Create a new SingleTileAttack if none exists for the entity
    this->singleTileAttack.push_back(new SingleTileAttack(spawnedBy));
    return this->singleTileAttack.back();
}

// Returns a list of objects representing all projectiles for rendering or collision detection
std::vector<Object *> AttackManager::getObjects() const
{
    std::vector<Object *> ret;

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
    for (const auto &a : this->singleTileAttack)
    {
        auto v = a->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    return ret;
}
