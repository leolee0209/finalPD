#pragma once
#include <vector>
#include <algorithm> // Added for std::sort
#include "tiles.hpp"
class Inventory
{
private:
    std::vector<Tile> tiles;

public:
    std::vector<Tile> &getTiles() { return this->tiles; }
    void CreatePlayerHand();
    
    // Find a tile by its unique ID. Returns pointer or nullptr if not found.
    Tile* getTileById(int id) {
        for (auto &t : tiles) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }

    void sortHand();
    
    Inventory(/* args */);
    ~Inventory();
};

inline void Inventory::sortHand()
{
    // Custom sort order: Character -> Dot -> Bamboo -> Wind -> Dragon
    // Within type: Small to big number (handled by enum order for same suit)
    // Suit order needs custom comparator because enum order is Dot->Bamboo->Character
    
    auto getSuitPriority = [](TileType t) -> int {
        if (t >= TileType::CHARACTER_1 && t <= TileType::CHARACTER_9) return 0;
        if (t >= TileType::DOT_1 && t <= TileType::DOT_9) return 1;
        if (t >= TileType::BAMBOO_1 && t <= TileType::BAMBOO_9) return 2;
        if (t >= TileType::WIND_EAST && t <= TileType::WIND_NORTH) return 3;
        if (t >= TileType::DRAGON_RED && t <= TileType::DRAGON_WHITE) return 4;
        return 5; // Others
    };

    std::sort(this->tiles.begin(), this->tiles.end(), [&](const Tile& a, const Tile& b) {
        int suitA = getSuitPriority(a.type);
        int suitB = getSuitPriority(b.type);
        
        if (suitA != suitB) {
            return suitA < suitB;
        }
        
        // Same suit, sort by enum value (which is ordered 1-9 for suits)
        return a.type < b.type;
    });
}

inline void Inventory::CreatePlayerHand()
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
    
    // Fill hand with 13-14 random tiles
    int handSize = 14; 
    
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
            // Default stats for starter hand
            this->tiles.push_back(Tile(TileStats(), type));
        }
    }
    
    this->sortHand();
}

inline Inventory::Inventory(/* args */)
{
}

inline Inventory::~Inventory()
{
}
