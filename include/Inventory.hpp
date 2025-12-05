#pragma once
#include <vector>
#include "tiles.hpp"
class Inventory
{
private:
    std::vector<Tile> tiles;

public:
    std::vector<Tile> &getTiles() { return this->tiles; }
    void CreatePlayerHand();
    Inventory(/* args */);
    ~Inventory();
};

inline void Inventory::CreatePlayerHand()
{
    this->tiles.clear();

    // Phase 3 testing setup - skills for each suit
    // Character suit: Dragon Claw (basic), Seismic Slam (2-2-2), Dash (1-2-3)
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_2));
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_2));
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_2));
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_3));
    // Bamboo suit: Rapid Throw (basic), Bamboo Bomb (2-2-2), Fan Shot (1-2-3)
    this->tiles.push_back(Tile(TileStats(), TileType::BAMBOO_1));
    this->tiles.push_back(Tile(TileStats(), TileType::BAMBOO_2));
    this->tiles.push_back(Tile(TileStats(), TileType::BAMBOO_3));
    // Dot suit: Arcane Orb (basic)
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_1));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_1));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_1));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_1));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_2));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_3));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_2));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_2));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_2));
}

inline Inventory::Inventory(/* args */)
{
}

inline Inventory::~Inventory()
{
}
