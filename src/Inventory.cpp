#include "Inventory.hpp"
#include "attackManager.hpp"
#include <vector>
#include <algorithm>
#include <iostream>
#include "raylib.h"

void Inventory::CreatePlayerHand()
{
    this->tiles.clear();

    // Allowed pool: 1,2,3 of Char/Bam/Dot, plus Dragons and Winds
    const TileType pool[] = {
        TileType::CHARACTER_1, TileType::CHARACTER_2, TileType::CHARACTER_3,
        TileType::BAMBOO_1,    TileType::BAMBOO_2,    TileType::BAMBOO_3,
        TileType::DOT_1,       TileType::DOT_2,       TileType::DOT_3,
        TileType::DRAGON_RED,  TileType::DRAGON_GREEN, TileType::DRAGON_WHITE,
        TileType::WIND_EAST,   TileType::WIND_SOUTH,   TileType::WIND_WEST, TileType::WIND_NORTH
    };
    int poolSize = sizeof(pool) / sizeof(TileType);

    // Track counts to enforce max 4 per type
    int counts[(int)TileType::TILE_COUNT] = {0};
    
    // Fill hand with 17 random tiles
    int handSize = 17; 
    
    for (int i = 0; i < handSize; ++i)
    {
        TileType type;
        int attempts = 0;
        do {
            type = pool[GetRandomValue(0, poolSize - 1)];
            attempts++;
        } while (counts[(int)type] >= 4 && attempts < 100);
        
        if (counts[(int)type] < 4) {
            counts[(int)type]++;
            this->tiles.push_back(Tile(TileStats(), type));
        } else {
             // Fallback
             this->tiles.push_back(Tile(TileStats(), pool[0]));
        }
    }

    // Ensure at least 2 combos
    int maxIterations = 1000;
    while (maxIterations-- > 0) {
        std::vector<bool> used(this->tiles.size(), false);
        int combosFound = 0;
        
        // Greedy search for disjoint combos
        for (size_t i = 0; i < this->tiles.size(); ++i) {
            if (used[i]) continue;
            for (size_t j = i + 1; j < this->tiles.size(); ++j) {
                if (used[j]) continue;
                for (size_t k = j + 1; k < this->tiles.size(); ++k) {
                    if (used[k]) continue;
                    
                    // Check combo using AttackManager
                    std::vector<SlotTileEntry> entries;
                    SlotTileEntry e1; e1.tile = this->tiles[i].type; e1.tileId = 1;
                    SlotTileEntry e2; e2.tile = this->tiles[j].type; e2.tileId = 2;
                    SlotTileEntry e3; e3.tile = this->tiles[k].type; e3.tileId = 3;
                    entries.push_back(e1);
                    entries.push_back(e2);
                    entries.push_back(e3);
                    
                    std::string attackType = AttackManager::classifyAttackType(entries);
                    
                    if (attackType != "NA" && attackType != "DefaultThrow") {
                        used[i] = used[j] = used[k] = true;
                        combosFound++;
                        goto next_combo;
                    }
                }
            }
            next_combo:;
        }
        
        if (combosFound >= 2) break;
        
        // If not enough combos, change a random UNUSED tile
        std::vector<int> unusedIndices;
        for (size_t i = 0; i < this->tiles.size(); ++i) {
            if (!used[i]) unusedIndices.push_back(i);
        }
        
        if (!unusedIndices.empty()) {
            int idx = unusedIndices[GetRandomValue(0, unusedIndices.size() - 1)];
            
            // Decrement old count
            counts[(int)this->tiles[idx].type]--;
            
            // Pick new valid tile
            TileType newType;
            int attempts = 0;
            do {
                newType = pool[GetRandomValue(0, poolSize - 1)];
                attempts++;
            } while (counts[(int)newType] >= 4 && attempts < 100);
            
            this->tiles[idx].type = newType;
            counts[(int)newType]++;
        }
    }
    
    this->sortHand();
}
