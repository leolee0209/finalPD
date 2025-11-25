#include "attackManager.hpp"
#include <iostream>

// Destructor cleans up dynamically allocated ThousandAttack instances
AttackManager::~AttackManager()
 {
    for (auto &a : this->singleTileAttack)
        delete a;
}

// Updates all ThousandAttack instances
void AttackManager::update(UpdateContext& uc)
{
    for (auto &a : this->singleTileAttack)
        a->update(uc);
}

void AttackManager::recordThrow(UpdateContext &uc)
{
    // Record the thrown tile (spawn already done by ThousandTileAttack::spawnProjectile)
    if (uc.uiManager)
    {
        MahjongTileType tile = uc.uiManager->muim.getSelectedTile();
        Rectangle rect = uc.uiManager->muim.getTile(tile);
        this->thrownTiles.push_back({tile, rect});
    }
    // Check for combos using the player's history
    this->checkActivation(uc.player);
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
        ThousandTileAttack *singleAttack = getSingleTileAttack(player);
        if (singleAttack)
        {
            singleAttack->startThousandMode();
        }
        combo_found = true;
    }
    // Triplet attack: 3 different character tiles in order
    else if (t1_val >= char_1 && t1_val <= char_9 &&
        t2_val >= char_1 && t2_val <= char_9 &&
        t3_val >= char_1 && t3_val <= char_9)
    {
        if (t1_val + 1 == t2_val && t2_val + 1 == t3_val)
        {
            ThousandTileAttack *singleAttack = getSingleTileAttack(player);
            if (singleAttack)
            {
                singleAttack->startTripletMode();
            }
            combo_found = true;
        }
    }

    if (combo_found)
    {
        thrownTiles.clear();
    }
}

ThousandTileAttack *AttackManager::getSingleTileAttack(Entity *spawnedBy)
{
    for (const auto &a : this->singleTileAttack)
    {
        if (a->spawnedBy == spawnedBy)
            return a;
    }
    // Create a new ThousandTileAttack if none exists for the entity
    this->singleTileAttack.push_back(new ThousandTileAttack(spawnedBy));
    return this->singleTileAttack.back();
}

std::vector<Entity *> AttackManager::getEntities()
{
    std::vector<Entity *> ret;
    for (const auto &a : this->singleTileAttack)
    {
        auto v = a->getEntities();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    return ret;
}

// Returns a list of objects representing all projectiles for rendering or collision detection
std::vector<Object *> AttackManager::getObjects() const
{
    std::vector<Object *> ret;
    for (const auto &a : this->singleTileAttack)
    {
        auto v = a->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    return ret;
}
