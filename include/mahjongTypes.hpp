#pragma once

/**
 * @brief Enumeration of Mahjong tile types used by the UI sprite sheet.
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

    TILE_COUNT
};

struct SlotTileEntry
{
    MahjongTileType tile = MahjongTileType::EMPTY;
    int handIndex = -1;
    bool isValid() const { return tile != MahjongTileType::EMPTY && handIndex >= 0; }
};
