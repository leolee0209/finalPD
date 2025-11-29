#pragma once
#include <raylib.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>
#include "uiElement.hpp"
#include "mahjongTypes.hpp"

/**
 * @brief Enumeration of Mahjong tile types used by the UI sprite sheet.
 *
 * `TILE_COUNT` can be used to size arrays referencing the full tile set.
 */
class AttackSlotElement;

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
        tileHitboxes.clear();
        tileUsed.clear();
    }

    void addTile(MahjongTileType type, Vector2 position);
    void createPlayerHand(int screenWidth, int screenHeight);
    void update();
    void draw();
    void nextTile();
    void previousTile();
    MahjongTileType getSelectedTile();
    void selectTileByType(MahjongTileType type);
    void selectTileByIndex(int index);
    MahjongTileType getTileTypeAt(int index) const;
    int getTileIndexAt(Vector2 position) const;
    Rectangle getTileBounds(int index) const;
    const std::vector<MahjongTileType> &getPlayerHand() const { return playerHand; }
    bool isTileUsed(int index) const;
    void setTileUsed(int index, bool used);

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
    std::vector<Rectangle> tileHitboxes;
    std::vector<bool> tileUsed;
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

    UIManager(const char *mahjongSpritePath, int tilesPerRow, int tileWidth, int tileHeight)
        : muim(MahjongUIManager(mahjongSpritePath, tilesPerRow, tileWidth, tileHeight)) {}
    ~UIManager() = default;
    void cleanup();

    void update();
    void draw();
    void addElement(UIElement *element);

    void setPauseMenuVisible(bool visible);
    bool isPauseMenuVisible() const { return pauseMenuVisible; }
    bool consumeResumeRequest();
    bool consumeQuitRequest();
    const std::vector<SlotTileEntry> &getSlotEntries(int slotIndex) const;

    MahjongUIManager muim;

private:
    static Texture2D loadTexture(const std::string &fileName);

    void updateHud();
    void updatePauseMenu();
    void drawHud();
    void drawPauseMenu();

    Rectangle getSmallButtonRect(int index) const;
    void drawSmallButton(const Rectangle &rect, const char *label, bool hovered) const;

    Rectangle getSlotRect(int index) const;
    void drawDraggingTile();
    void ensureSlotSetup();             // Initializes slot data from the hand on first use
    void ensureSlotElements();          // Lazily instantiates AttackSlotElement widgets
    void updateSlotElementLayout();     // Recomputes bounds + bindings every frame
    AttackSlotElement *getSlotElement(int index) const;

    void beginTileDragFromHand(int tileIndex, const Vector2 &mousePos);
    void beginTileDragFromSlot(int slotIndex, int tileIndex, const Vector2 &mousePos);
    void endTileDrag(const Vector2 &mousePos);
    void addTileToSlot(int slotIndex, const SlotTileEntry &entry);

    bool slotHasSpace(int slotIndex) const;
    bool isValidSlotIndex(int slotIndex) const;
    MahjongTileType getPrimaryTileForSlot(int slotIndex) const;
    void refreshActiveSlotSelection();

    std::vector<UIElement *> elements;
    bool pauseMenuVisible = false;
    bool resumeRequested = false;
    bool quitRequested = false;
    static constexpr int slotCapacity = 3;
    static constexpr int slotCount = 3;
    std::array<std::vector<SlotTileEntry>, slotCount> attackSlots;
    std::array<AttackSlotElement *, slotCount> slotElements{}; // Owned UI wrappers for each slot
    int activeSlotIndex = 0;
    bool slotsInitialized = false;
    bool isDraggingTile = false;
    bool draggingFromHand = false;
    int draggingFromSlot = -1;
    int draggingFromTileIndex = -1;
    SlotTileEntry draggingTile;
    Vector2 draggingTilePos{0.0f, 0.0f};
};
