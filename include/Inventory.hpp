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

    // const float startY = screenHeight - this->tileHeight - 10.0f; // 10px margin from bottom

    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_1));
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_2));
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_3));
    this->tiles.push_back(Tile(TileStats(), TileType::BAMBOO_6));
    this->tiles.push_back(Tile(TileStats(), TileType::BAMBOO_6));
    this->tiles.push_back(Tile(TileStats(), TileType::BAMBOO_6));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_9));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_9));
    this->tiles.push_back(Tile(TileStats(), TileType::DOT_9));
    this->tiles.push_back(Tile(TileStats(), TileType::WIND_EAST));
    this->tiles.push_back(Tile(TileStats(), TileType::WIND_WEST));
    this->tiles.push_back(Tile(TileStats(), TileType::DRAGON_RED));
    this->tiles.push_back(Tile(TileStats(), TileType::DRAGON_GREEN));
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_9));
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_9));
    this->tiles.push_back(Tile(TileStats(), TileType::CHARACTER_9));
    // playerHand.push_back(MahjongTileType::BAMBOO_1);

    // Initialize default stats for each tile
    // for (size_t i = 0; i < playerHand.size(); ++i)
    // {
    //     // Randomize stats slightly for variety
    //     float damage = 8.0f + (float)(rand() % 5);             // 8-12 damage
    //     float fireRate = 0.8f + ((float)(rand() % 5) / 10.0f); // 0.8-1.2 fire rate
    //     this->tileStats.push_back(TileStats(damage, fireRate));
    // }

    // const int handSize = playerHand.size();
    // const float totalHandWidth = handSize * this->tileWidth;
    // const float startX = (screenWidth - totalHandWidth) / 2.0f;
}

inline Inventory::Inventory(/* args */)
{
}

inline Inventory::~Inventory()
{
}
