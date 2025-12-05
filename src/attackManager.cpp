#include "attackManager.hpp"
#include <array>
#include <algorithm>
#include <iostream>
#include <string>

namespace
{
bool isCharacterTile(TileType tile)
{
    return tile >= TileType::CHARACTER_1 && tile <= TileType::CHARACTER_9;
}

int getCharacterValue(TileType tile)
{
    return static_cast<int>(tile) - static_cast<int>(TileType::CHARACTER_1) + 1;
}

int getBambooValue(TileType tile)
{
    return static_cast<int>(tile) - static_cast<int>(TileType::BAMBOO_1) + 1;
}

bool isDotTile(TileType tile)
{
    return tile >= TileType::DOT_1 && tile <= TileType::DOT_9;
}

bool isBambooTile(TileType tile)
{
    return tile >= TileType::BAMBOO_1 && tile <= TileType::BAMBOO_9;
}

// Determine the attack archetype based on the selected tile
enum class AttackArchetype
{
    MELEE,    // Character suit, Red Dragon, Winds
    RANGER,   // Bamboo suit, Green Dragon
    MAGE      // Dot suit, White Dragon
};

AttackArchetype getAttackArchetype(TileType tile)
{
    // Character tiles -> Melee
    if (isCharacterTile(tile))
        return AttackArchetype::MELEE;
    
    // Bamboo tiles -> Ranger
    if (isBambooTile(tile))
        return AttackArchetype::RANGER;
    
    // Dot tiles -> Mage
    if (isDotTile(tile))
        return AttackArchetype::MAGE;
    
    // Dragon and Wind tiles
    switch (tile)
    {
        case TileType::DRAGON_RED:
        case TileType::WIND_EAST:
        case TileType::WIND_SOUTH:
        case TileType::WIND_WEST:
        case TileType::WIND_NORTH:
            return AttackArchetype::MELEE;
        
        case TileType::DRAGON_GREEN:
            return AttackArchetype::RANGER;
        
        case TileType::DRAGON_WHITE:
            return AttackArchetype::MAGE;
        
        default:
            // Default to Ranger for other tiles
            return AttackArchetype::RANGER;
    }
}
}

// Destructor cleans up dynamically allocated ThousandAttack instances
AttackManager::~AttackManager()
 {
    for (auto &a : this->basicTileAttacks)
        delete a;
    for (auto &a : this->singleTileAttack)
        delete a;
    for (auto &m : this->meleeAttacks)
        delete m;
    for (auto &d : this->dashAttacks)
        delete d;
    for (auto &b : this->bambooBombAttacks)
        delete b;
    for (auto &b : this->bambooTripleAttacks)
        delete b;
    for (auto &d : this->dragonClawAttacks)
        delete d;
    for (auto &a : this->arcaneOrbAttacks)
        delete a;
    for (auto &f : this->fanShotAttacks)
        delete f;
    for (auto &s : this->seismicSlamAttacks)
        delete s;
}

// Updates all ThousandAttack instances
void AttackManager::update(UpdateContext& uc)
{
    for (auto &a : this->basicTileAttacks)
        a->update(uc);
    for (auto &a : this->singleTileAttack)
        a->update(uc);
    for (auto &m : this->meleeAttacks)
        m->update(uc);
    for (auto &d : this->dashAttacks)
        d->update(uc);
    for (auto &b : this->bambooBombAttacks)
        b->update(uc);
    for (auto &b : this->bambooTripleAttacks)
        b->update(uc);
    for (auto &d : this->dragonClawAttacks)
        d->update(uc);
    for (auto &a : this->arcaneOrbAttacks)
        a->update(uc);
    for (auto &f : this->fanShotAttacks)
        f->update(uc);
    for (auto &s : this->seismicSlamAttacks)
        s->update(uc);

    // Update BasicTileAttack cooldown modifiers based on BambooTripleAttack state
    for (auto &basic : this->basicTileAttacks)
    {
        // Find associated bamboo triple attack (same spawner)
        BambooTripleAttack *bamboo = nullptr;
        for (auto &b : this->bambooTripleAttacks)
        {
            if (b->spawnedBy == basic->spawnedBy)
            {
                bamboo = b;
                break;
            }
        }
        
        // Update cooldown modifier based on effect state
        if (bamboo && bamboo->isActive())
        {
            basic->setCooldownModifier(0.4f); // 40% = faster shooting
        }
        else
        {
            basic->resetCooldownModifier(); // Back to normal
        }
    }

    if (uc.uiManager)
    {
        for (int slotIdx = 0; slotIdx < UIManager::slotCount; ++slotIdx)
        {
            const auto &slotEntries = uc.uiManager->getSlotEntries(slotIdx);
            
            // Empty slots are valid (no red outline)
            if (slotEntries.empty())
            {
                uc.uiManager->setSlotValidity(slotIdx, true);
            }
            // Single tile or two tiles are invalid for skill slots
            else if (slotEntries.size() < 3)
            {
                uc.uiManager->setSlotValidity(slotIdx, false);
            }
            // Three or more tiles: check if they form a valid combo
            else
            {
                SlotAttackKind kind = classifySlotAttack(slotEntries);
                // Only DefaultThrow and None are invalid for 3+ tiles
                bool valid = (kind != SlotAttackKind::None && kind != SlotAttackKind::DefaultThrow);
                uc.uiManager->setSlotValidity(slotIdx, valid);
            }

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
        TileType tile = TileType::BAMBOO_1;
        if (uc.player)
        {
            tile = uc.uiManager->muim.getSelectedTile(uc.player->hand);
        }
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

    bool isBambooTriple = false;
    if (canCheckCombo && !isDotTriple)
    {
        const auto &first = slotEntries[0];
        if (first.isValid() && isBambooTile(first.tile))
        {
            isBambooTriple = true;
            for (int i = 1; i < 3; ++i)
            {
                if (!slotEntries[i].isValid() || slotEntries[i].tile != first.tile)
                {
                    isBambooTriple = false;
                    break;
                }
            }
        }
    }

    if (isDotTriple)
    {
        if (BambooBombAttack *bomb = getBambooBombAttack(uc.player))
        {
            if (bomb->trigger(uc, slotEntries[0].tile))
                return true;
        }
        return false;
    }

    if (isBambooTriple)
    {
        if (BambooTripleAttack *bamboo = getBambooTripleAttack(uc.player))
        {
            bamboo->trigger(uc);
            // Activate the rapid-fire effect on the BasicTileAttack
            if (BasicTileAttack *basic = getBasicTileAttack(uc.player))
            {
                basic->setCooldownModifier(0.4f); // 40% of normal cooldown = faster shooting
            }
            return true;
        }
        return false;
    }

    // Check for FanShot combo (Bamboo 1-2-3 sequence)
    if (canCheckCombo)
    {
        if (slotEntries[0].isValid() && slotEntries[1].isValid() && slotEntries[2].isValid())
        {
            if (isBambooTile(slotEntries[0].tile) && isBambooTile(slotEntries[1].tile) && isBambooTile(slotEntries[2].tile))
            {
                int val0 = getBambooValue(slotEntries[0].tile);
                int val1 = getBambooValue(slotEntries[1].tile);
                int val2 = getBambooValue(slotEntries[2].tile);
                
                std::array<int, 3> sortedBamboo = {val0, val1, val2};
                std::sort(sortedBamboo.begin(), sortedBamboo.end());
                
                if (sortedBamboo[0] == 1 && sortedBamboo[1] == 2 && sortedBamboo[2] == 3)
                {
                    if (FanShotAttack *fanShot = getFanShotAttack(uc.player))
                    {
                        if (fanShot->trigger(uc))
                            return true;
                    }
                    return false;
                }
            }
        }
    }

    if (canCheckCombo && allValidCharacters)
    {
        // Check for SeismicSlam combo (Character 2-2-2)
        if (characterValues[0] == 2 && characterValues[1] == 2 && characterValues[2] == 2)
        {
            if (SeismicSlamAttack *slam = getSeismicSlamAttack(uc.player))
            {
                if (slam->trigger(uc))
                    return true;
            }
            return false;
        }
        
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
            if (MeleePushAttack *melee = getMeleePushAttack(uc.player))
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

    int char_1 = static_cast<int>(TileType::CHARACTER_1);
    int char_9 = static_cast<int>(TileType::CHARACTER_9);

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

std::string AttackManager::classifyAttackType(const std::vector<SlotTileEntry> &tiles)
{
    if (tiles.empty())
        return "NA";

    bool canCheckCombo = tiles.size() >= 3;
    std::array<int, 3> characterValues{};
    bool allValidCharacters = canCheckCombo;
    if (canCheckCombo)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto &entry = tiles[i];
            if (!entry.isValid() || !isCharacterTile(entry.tile))
            {
                allValidCharacters = false;
                break;
            }
            characterValues[i] = getCharacterValue(entry.tile);
        }
    }

    // Check Dot Triple (e.g., Dot 5-5-5)
    bool isDotTriple = false;
    if (canCheckCombo)
    {
        const auto &first = tiles[0];
        if (first.isValid() && isDotTile(first.tile))
        {
            isDotTriple = true;
            for (int i = 1; i < 3; ++i)
            {
                if (!tiles[i].isValid() || tiles[i].tile != first.tile)
                {
                    isDotTriple = false;
                    break;
                }
            }
        }
    }

    if (isDotTriple)
        return "DotBomb";

    // Check Bamboo Triple (e.g., Bamboo 3-3-3)
    bool isBambooTriple = false;
    if (canCheckCombo && !isDotTriple)
    {
        const auto &first = tiles[0];
        if (first.isValid() && isBambooTile(first.tile))
        {
            isBambooTriple = true;
            for (int i = 1; i < 3; ++i)
            {
                if (!tiles[i].isValid() || tiles[i].tile != first.tile)
                {
                    isBambooTriple = false;
                    break;
                }
            }
        }
    }

    if (isBambooTriple)
        return "BambooTriple";

    // Check for Bamboo FanShot combo (1-2-3 in any order)
    if (canCheckCombo)
    {
        if (tiles[0].isValid() && tiles[1].isValid() && tiles[2].isValid())
        {
            if (isBambooTile(tiles[0].tile) && isBambooTile(tiles[1].tile) && isBambooTile(tiles[2].tile))
            {
                int val0 = getBambooValue(tiles[0].tile);
                int val1 = getBambooValue(tiles[1].tile);
                int val2 = getBambooValue(tiles[2].tile);
                
                std::array<int, 3> sortedBamboo = {val0, val1, val2};
                std::sort(sortedBamboo.begin(), sortedBamboo.end());
                
                if (sortedBamboo[0] == 1 && sortedBamboo[1] == 2 && sortedBamboo[2] == 3)
                    return "FanShot";
            }
        }
    }

    // Check Character combos
    if (canCheckCombo && allValidCharacters)
    {
        // Seismic Slam: Character 2-2-2
        if (characterValues[0] == 2 && characterValues[1] == 2 && characterValues[2] == 2)
            return "SeismicSlam";
        
        // Melee: All same value
        bool isMeleeCombo = (characterValues[0] == characterValues[1]) && (characterValues[1] == characterValues[2]);

        // Dash: Sequential values in any order (e.g., 1-2-3, 3-4-5)
        auto sortedValues = characterValues;
        std::sort(sortedValues.begin(), sortedValues.end());
        bool isDashCombo = (sortedValues[0] + 1 == sortedValues[1]) && (sortedValues[1] + 1 == sortedValues[2]);

        if (isMeleeCombo)
            return "Melee";
        if (isDashCombo)
            return "Dash";
    }

    // Default throw for single valid tile
    if (tiles.front().isValid())
        return "DefaultThrow";

    return "NA";
}

AttackManager::SlotAttackKind AttackManager::classifySlotAttack(const std::vector<SlotTileEntry> &slotEntries) const
{
    // Use the static function and convert back to enum
    std::string attackType = classifyAttackType(slotEntries);
    
    if (attackType == "DotBomb") return SlotAttackKind::DotBomb;
    if (attackType == "BambooTriple") return SlotAttackKind::BambooTriple;
    if (attackType == "FanShot") return SlotAttackKind::FanShot;
    if (attackType == "SeismicSlam") return SlotAttackKind::SeismicSlam;
    if (attackType == "Melee") return SlotAttackKind::Melee;
    if (attackType == "Dash") return SlotAttackKind::Dash;
    if (attackType == "DefaultThrow") return SlotAttackKind::DefaultThrow;
    
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
        BambooBombAttack *bomb = getBambooBombAttack(uc.player);
        return bomb ? bomb->getCooldownPercent() : 0.0f;
    }
    case SlotAttackKind::BambooTriple:
    {
        BambooTripleAttack *bamboo = getBambooTripleAttack(uc.player);
        return bamboo ? bamboo->getCooldownPercent() : 0.0f;
    }
    case SlotAttackKind::Melee:
    {
        MeleePushAttack *melee = getMeleePushAttack(uc.player);
        return melee ? melee->getCooldownPercent() : 0.0f;
    }
    case SlotAttackKind::Dash:
    {
        DashAttack *dash = getDashAttack(uc.player);
        return dash ? dash->getCooldownPercent() : 0.0f;
    }
    case SlotAttackKind::FanShot:
    {
        FanShotAttack *fanShot = getFanShotAttack(uc.player);
        return fanShot ? fanShot->getCooldownPercent() : 0.0f;
    }
    case SlotAttackKind::SeismicSlam:
    {
        SeismicSlamAttack *slam = getSeismicSlamAttack(uc.player);
        return slam ? slam->getCooldownPercent() : 0.0f;
    }
    case SlotAttackKind::None:
    case SlotAttackKind::DefaultThrow:
    default:
        // Empty slots or default throws should show as ready (not gray)
        return 1.0f;
    }
}

BasicTileAttack *AttackManager::getBasicTileAttack(Entity *spawnedBy)
{
    for (const auto &a : this->basicTileAttacks)
    {
        if (a->spawnedBy == spawnedBy)
            return a;
    }
    // Create a new BasicTileAttack if none exists for the entity
    this->basicTileAttacks.push_back(new BasicTileAttack(spawnedBy));
    return this->basicTileAttacks.back();
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

MeleePushAttack *AttackManager::getMeleePushAttack(Entity *spawnedBy)
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

BambooTripleAttack *AttackManager::getBambooTripleAttack(Entity *spawnedBy)
{
    for (const auto &b : this->bambooTripleAttacks)
    {
        if (b->spawnedBy == spawnedBy)
            return b;
    }
    this->bambooTripleAttacks.push_back(new BambooTripleAttack(spawnedBy));
    return this->bambooTripleAttacks.back();
}

BambooBombAttack *AttackManager::getBambooBombAttack(Entity *spawnedBy)
{
    for (const auto &b : this->bambooBombAttacks)
    {
        if (b->spawnedBy == spawnedBy)
            return b;
    }
    this->bambooBombAttacks.push_back(new BambooBombAttack(spawnedBy));
    return this->bambooBombAttacks.back();
}

std::vector<Entity *> AttackManager::getEntities(EntityCategory cat)
{
    std::vector<Entity *> ret;
    // If caller requests projectiles or all entities, include projectiles
    if (cat == ENTITY_PROJECTILE || cat == ENTITY_ALL)
    {
        for (const auto &a : this->basicTileAttacks)
        {
            auto v = a->getEntities();
            ret.insert(ret.end(), v.begin(), v.end());
        }
        for (const auto &a : this->singleTileAttack)
        {
            auto v = a->getEntities();
            ret.insert(ret.end(), v.begin(), v.end());
        }
        for (const auto &b : this->bambooBombAttacks)
        {
            auto v = b->getEntities();
            ret.insert(ret.end(), v.begin(), v.end());
        }
        for (const auto &f : this->fanShotAttacks)
        {
            auto v = f->getEntities();
            ret.insert(ret.end(), v.begin(), v.end());
        }
    }
    return ret;
}

// Returns a list of objects representing all projectiles for rendering or collision detection
std::vector<Object *> AttackManager::getObjects() const
{
    std::vector<Object *> ret;
    for (const auto &a : this->basicTileAttacks)
    {
        auto v = a->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
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
    for (const auto &b : this->bambooBombAttacks)
    {
        auto v = b->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    for (const auto &d : this->dragonClawAttacks)
    {
        auto v = d->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    for (const auto &a : this->arcaneOrbAttacks)
    {
        auto v = a->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    for (const auto &f : this->fanShotAttacks)
    {
        auto v = f->obj();
        ret.insert(ret.end(), v.begin(), v.end());
    }
    for (const auto &s : this->seismicSlamAttacks)
    {
        auto v = s->obj();
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

DragonClawAttack *AttackManager::getDragonClawAttack(Entity *spawnedBy)
{
    for (const auto &d : this->dragonClawAttacks)
    {
        if (d->spawnedBy == spawnedBy)
            return d;
    }
    this->dragonClawAttacks.push_back(new DragonClawAttack(spawnedBy));
    return this->dragonClawAttacks.back();
}

ArcaneOrbAttack *AttackManager::getArcaneOrbAttack(Entity *spawnedBy)
{
    for (const auto &a : this->arcaneOrbAttacks)
    {
        if (a->spawnedBy == spawnedBy)
            return a;
    }
    this->arcaneOrbAttacks.push_back(new ArcaneOrbAttack(spawnedBy));
    return this->arcaneOrbAttacks.back();
}

FanShotAttack *AttackManager::getFanShotAttack(Entity *spawnedBy)
{
    for (const auto &f : this->fanShotAttacks)
    {
        if (f->spawnedBy == spawnedBy)
            return f;
    }
    this->fanShotAttacks.push_back(new FanShotAttack(spawnedBy));
    return this->fanShotAttacks.back();
}

SeismicSlamAttack *AttackManager::getSeismicSlamAttack(Entity *spawnedBy)
{
    for (const auto &s : this->seismicSlamAttacks)
    {
        if (s->spawnedBy == spawnedBy)
            return s;
    }
    this->seismicSlamAttacks.push_back(new SeismicSlamAttack(spawnedBy));
    return this->seismicSlamAttacks.back();
}