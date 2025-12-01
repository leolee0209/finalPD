#include "attackManager.hpp"
#include <array>
#include <algorithm>
#include <iostream>

namespace
{
bool isCharacterTile(MahjongTileType tile)
{
    return tile >= MahjongTileType::CHARACTER_1 && tile <= MahjongTileType::CHARACTER_9;
}

int getCharacterValue(MahjongTileType tile)
{
    return static_cast<int>(tile) - static_cast<int>(MahjongTileType::CHARACTER_1) + 1;
}

bool isDotTile(MahjongTileType tile)
{
    return tile >= MahjongTileType::DOT_1 && tile <= MahjongTileType::DOT_9;
}
}

// Destructor cleans up dynamically allocated ThousandAttack instances
AttackManager::~AttackManager()
 {
    for (auto &a : this->singleTileAttack)
        delete a;
    for (auto &m : this->meleeAttacks)
        delete m;
    for (auto &d : this->dashAttacks)
        delete d;
    for (auto &b : this->dotBombAttacks)
        delete b;
}

// Updates all ThousandAttack instances
void AttackManager::update(UpdateContext& uc)
{
    for (auto &a : this->singleTileAttack)
        a->update(uc);
    for (auto &m : this->meleeAttacks)
        m->update(uc);
    for (auto &d : this->dashAttacks)
        d->update(uc);
    for (auto &b : this->dotBombAttacks)
        b->update(uc);

    if (uc.uiManager)
    {
        for (int slotIdx = 0; slotIdx < UIManager::slotCount; ++slotIdx)
        {
            float percent = computeSlotCooldownPercent(slotIdx, uc);
            uc.uiManager->setSlotCooldownPercent(slotIdx, percent);
        }
    }
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

bool AttackManager::triggerSlotAttack(int slotIndex, UpdateContext &uc)
{
    if (!uc.player || !uc.uiManager)
        return false;

    const auto &slotEntries = uc.uiManager->getSlotEntries(slotIndex);
    if (slotEntries.empty())
        return false;

    const bool canCheckCombo = slotEntries.size() >= 3;
    std::array<int, 3> characterValues{};
    bool allValidCharacters = canCheckCombo;
    if (canCheckCombo)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto &entry = slotEntries[i];
            if (!entry.isValid() || !isCharacterTile(entry.tile))
            {
                allValidCharacters = false;
                break;
            }
            characterValues[i] = getCharacterValue(entry.tile);
        }
    }

    bool isDotTriple = false;
    if (canCheckCombo)
    {
        const auto &first = slotEntries[0];
        if (first.isValid() && isDotTile(first.tile))
        {
            isDotTriple = true;
            for (int i = 1; i < 3; ++i)
            {
                if (!slotEntries[i].isValid() || slotEntries[i].tile != first.tile)
                {
                    isDotTriple = false;
                    break;
                }
            }
        }
    }

    if (isDotTriple)
    {
        if (DotBombAttack *bomb = getDotBombAttack(uc.player))
        {
            if (bomb->trigger(uc, slotEntries[0].tile))
                return true;
        }
        return false;
    }

    if (canCheckCombo && allValidCharacters)
    {
        bool isMeleeCombo = (characterValues[0] == characterValues[1]) && (characterValues[1] == characterValues[2]);
        bool isDashCombo = false;
        if (!isMeleeCombo)
        {
            auto sortedValues = characterValues;
            std::sort(sortedValues.begin(), sortedValues.end());
            isDashCombo = (sortedValues[0] + 1 == sortedValues[1]) && (sortedValues[1] + 1 == sortedValues[2]);
        }

        if (isMeleeCombo)
        {
            if (MeleePushAttack *melee = getMeleeAttack(uc.player))
            {
                melee->trigger(uc);
                return true;
            }
            return false;
        }

        if (isDashCombo)
        {
            if (DashAttack *dash = getDashAttack(uc.player))
            {
                dash->trigger(uc);
                return true;
            }
            return false;
        }
    }

    return false;
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

AttackManager::SlotAttackKind AttackManager::classifySlotAttack(const std::vector<SlotTileEntry> &slotEntries) const
{
    if (slotEntries.empty())
        return SlotAttackKind::None;

    bool canCheckCombo = slotEntries.size() >= 3;
    std::array<int, 3> characterValues{};
    bool allValidCharacters = canCheckCombo;
    if (canCheckCombo)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto &entry = slotEntries[i];
            if (!entry.isValid() || !isCharacterTile(entry.tile))
            {
                allValidCharacters = false;
                break;
            }
            characterValues[i] = getCharacterValue(entry.tile);
        }
    }

    bool isDotTriple = false;
    if (canCheckCombo)
    {
        const auto &first = slotEntries[0];
        if (first.isValid() && isDotTile(first.tile))
        {
            isDotTriple = true;
            for (int i = 1; i < 3; ++i)
            {
                if (!slotEntries[i].isValid() || slotEntries[i].tile != first.tile)
                {
                    isDotTriple = false;
                    break;
                }
            }
        }
    }

    if (isDotTriple)
        return SlotAttackKind::DotBomb;

    if (canCheckCombo && allValidCharacters)
    {
        bool isMeleeCombo = (characterValues[0] == characterValues[1]) && (characterValues[1] == characterValues[2]);

        auto sortedValues = characterValues;
        std::sort(sortedValues.begin(), sortedValues.end());
        bool isDashCombo = (sortedValues[0] + 1 == sortedValues[1]) && (sortedValues[1] + 1 == sortedValues[2]);

        if (isMeleeCombo)
            return SlotAttackKind::Melee;
        if (isDashCombo)
            return SlotAttackKind::Dash;
    }

    if (slotEntries.front().isValid())
        return SlotAttackKind::DefaultThrow;

    return SlotAttackKind::None;
}

float AttackManager::computeSlotCooldownPercent(int slotIndex, UpdateContext &uc)
{
    if (!uc.uiManager || !uc.player)
        return 0.0f;

    const auto &slotEntries = uc.uiManager->getSlotEntries(slotIndex);
    SlotAttackKind kind = classifySlotAttack(slotEntries);
    switch (kind)
    {
    case SlotAttackKind::DotBomb:
    {
        DotBombAttack *bomb = getDotBombAttack(uc.player);
        return bomb ? bomb->getCooldownPercent() : 0.0f;
    }
    case SlotAttackKind::Melee:
    {
        MeleePushAttack *melee = getMeleeAttack(uc.player);
        return melee ? melee->getCooldownPercent() : 0.0f;
    }
    case SlotAttackKind::Dash:
    {
        DashAttack *dash = getDashAttack(uc.player);
        return dash ? dash->getCooldownPercent() : 0.0f;
    }
    default:
        return 0.0f;
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

MeleePushAttack *AttackManager::getMeleeAttack(Entity *spawnedBy)
{
    for (const auto &m : this->meleeAttacks)
    {
        if (m->spawnedBy == spawnedBy)
            return m;
    }
    this->meleeAttacks.push_back(new MeleePushAttack(spawnedBy));
    return this->meleeAttacks.back();
}

DashAttack *AttackManager::getDashAttack(Entity *spawnedBy)
{
    for (const auto &d : this->dashAttacks)
    {
        if (d->spawnedBy == spawnedBy)
            return d;
    }
    this->dashAttacks.push_back(new DashAttack(spawnedBy));
    return this->dashAttacks.back();
}

DotBombAttack *AttackManager::getDotBombAttack(Entity *spawnedBy)
{
    for (const auto &b : this->dotBombAttacks)
    {
        if (b->spawnedBy == spawnedBy)
            return b;
    }
    this->dotBombAttacks.push_back(new DotBombAttack(spawnedBy));
    return this->dotBombAttacks.back();
}

std::vector<Entity *> AttackManager::getEntities(EntityCategory cat)
{
    std::vector<Entity *> ret;
    // If caller requests projectiles or all entities, include projectiles
    if (cat == ENTITY_PROJECTILE || cat == ENTITY_ALL)
    {
        for (const auto &a : this->singleTileAttack)
        {
            auto v = a->getEntities();
            ret.insert(ret.end(), v.begin(), v.end());
        }
        for (const auto &b : this->dotBombAttacks)
        {
            auto v = b->getEntities();
            ret.insert(ret.end(), v.begin(), v.end());
        }
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
    for (const auto &m : this->meleeAttacks)
    {
        auto v = m->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    for (const auto &b : this->dotBombAttacks)
    {
        auto v = b->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    // dash attack currently has no objects to render
    return ret;
}

bool AttackManager::isAttackLockedByOther(const AttackController *controller) const
{
    return this->attackLockOwner && this->attackLockOwner != controller;
}

bool AttackManager::tryLockAttack(AttackController *controller)
{
    if (!controller)
        return false;
    if (this->attackLockOwner && this->attackLockOwner != controller)
        return false;
    this->attackLockOwner = controller;
    return true;
}

void AttackManager::releaseAttackLock(const AttackController *controller)
{
    if (this->attackLockOwner == controller)
    {
        this->attackLockOwner = nullptr;
    }
}
