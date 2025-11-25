#pragma once
#include <raylib.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "uiElement.hpp"

/**
 * @brief Enumeration of Mahjong tile types used by the UI sprite sheet.
 *
 * `TILE_COUNT` can be used to size arrays referencing the full tile set.
 */
enum class MahjongTileType
{
    // Dot suit (Tong)
    DOT_1,
    DOT_2,
    DOT_3,
    DOT_4,
    DOT_5,
    DOT_6,
    DOT_7,
    DOT_8,
    DOT_9,
    // Bamboo suit (Suo)
    BAMBOO_1,
    BAMBOO_2,
    BAMBOO_3,
    BAMBOO_4,
    BAMBOO_5,
    BAMBOO_6,
    BAMBOO_7,
    BAMBOO_8,
    BAMBOO_9,
    // Character suit (Wan)
    CHARACTER_1,
    CHARACTER_2,
    CHARACTER_3,
    CHARACTER_4,
    CHARACTER_5,
    CHARACTER_6,
    CHARACTER_7,
    CHARACTER_8,
    CHARACTER_9,
    // Winds
    WIND_EAST,
    WIND_SOUTH,
    WIND_WEST,
    WIND_NORTH,
    // Dragons
    DRAGON_RED,
    DRAGON_GREEN,
    DRAGON_WHITE,
    // Others
    BACK,
    EMPTY,
    // Seasons
    SEASON_SPRING,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    // Flowers
    FLOWER_PLUM,
    FLOWER_ORCHID,
    FLOWER_CHRYSANTHEMUM,
    FLOWER_BAMBOO,
    
    TILE_COUNT // Not a tile, but gives the total number of tile types
};

/**
 * @brief Helper to manage the Mahjong tile sprite sheet and player hand UI.
 *
 * MahjongUIManager loads a tile spritesheet and provides helpers to create
 * a simple player hand UI, select tiles and return source rectangles for
 * drawing individual tiles.
 */
class MahjongUIManager
{
public:
    MahjongUIManager(const char *mahjongSpritePath, int _tilesPerRow, int _tileWidth, int _tileHeight)
        : tilesPerRow(_tilesPerRow), tileWidth(_tileWidth), tileHeight(_tileHeight)
    {
        spriteSheet = LoadTexture(mahjongSpritePath);
    }
    ~MahjongUIManager() = default;
    void cleanup()
    {
        UnloadTexture(spriteSheet);
        for (auto &e : this->elements)
        {
            delete e;
        }
        elements.clear();
    }

    void addTile(MahjongTileType type, Vector2 position);
    void createPlayerHand(int screenWidth, int screenHeight);
    void update();
    void draw();
    void nextTile();
    void previousTile();
    MahjongTileType getSelectedTile();

    // Returns the source rectangle for a given tile type.
    Rectangle getTile(MahjongTileType type)
    {
        int index = static_cast<int>(type);
        int row = index / this->tilesPerRow;
        int col = index % this->tilesPerRow;

        float x = static_cast<float>(col * this->tileWidth);
        float y = static_cast<float>(row * this->tileHeight);

        return {x, y, static_cast<float>(this->tileWidth), static_cast<float>(this->tileHeight)};
    }

    Texture2D& getSpriteSheet() { return spriteSheet; }

private:
    int selectedTileIndex = 0;
    std::vector<MahjongTileType> playerHand;
    int tilesPerRow;
    int tileWidth, tileHeight;
    Texture2D spriteSheet;
    std::vector<UIElement *> elements;
};

/**
 * @brief High-level UI facade used by game systems.
 *
 * Holds a `MahjongUIManager` (tile sprites), a list of additional
 * `UIElement`s and provides `update()`/`draw()` entry points called each
 * frame by the main loop.
 */
class UIManager
{
public:
    UIManager(const char *mahjongSpritePath, int tilesPerRow, int tileWidth, int tileHeight) : muim(MahjongUIManager(mahjongSpritePath, tilesPerRow, tileWidth, tileHeight)) {}
    ~UIManager() = default;
    void cleanup();

    void update();
    void draw();
    void addElement(UIElement *element);

    MahjongUIManager muim;

private:
    static Texture2D loadTexture(const std::string &fileName);

    std::vector<UIElement *> elements;
};
