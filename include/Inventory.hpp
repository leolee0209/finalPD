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

    void CreatePlayerHand();

inline Inventory::Inventory(/* args */)
{
}

inline Inventory::~Inventory()
{
}
