#pragma once
#include <raylib.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>
#include "updateContext.hpp"
#include <algorithm>
#include "uiElement.hpp"
#include "tiles.hpp"
#include "Inventory.hpp"
class RewardBriefcase;

/**
 * @brief Enumeration of Mahjong tile types used by the UI sprite sheet.
 *
 * `TILE_COUNT` can be used to size arrays referencing the full tile set.
 */
class AttackSlotElement;

/**
 * @brief Stores position and size information for a single briefcase tile UI element.
 */
struct BriefcaseTileUI
{
    Rectangle rect;
    TileType tileType;
    int inventoryIndex;
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
        for (auto &e : this->handElements)
        {
            delete e;
        }
        handElements.clear();
        tileHitboxes.clear();
        tileUsed.clear();
    }

    // Build UI elements from an inventory (call in update)
    void createHandUI(Inventory &inventory, int screenWidth, int screenHeight);
    void update(Inventory &inventory);
    void draw();
    void nextTile(Inventory &inventory);
    void previousTile(Inventory &inventory);

    TileType getSelectedTile(Inventory &inventory);
    void selectTileByType(Inventory &inventory, TileType type);
    void selectTileByIndex(int index);
    int getTileIndexAt(Vector2 position) const;

    Rectangle getTileBounds(int index) const;
    bool isTileUsed(int index) const;
    void setTileUsed(int index, bool used);
    int getTileCount() const { return static_cast<int>(tileHitboxes.size()); }
    Rectangle getTileRect(int index) const { return (index >= 0 && index < (int)tileHitboxes.size()) ? tileHitboxes[index] : Rectangle{0,0,0,0}; }

    int getSelectedTileIndex() const { return selectedTileIndex; }

    // Returns the source rectangle for a given tile type.
    Rectangle getTile(TileType type)
    {
        int index = static_cast<int>(type);
        int row = index / this->tilesPerRow;
        int col = index % this->tilesPerRow;

        float x = static_cast<float>(col * this->tileWidth);
        float y = static_cast<float>(row * this->tileHeight);

        return {x, y, static_cast<float>(this->tileWidth), static_cast<float>(this->tileHeight)};
    }

    Texture2D &getSpriteSheet() { return spriteSheet; }

private:
    int selectedTileIndex = 0;
    int tilesPerRow;
    int tileWidth, tileHeight;
    Texture2D spriteSheet;
    std::vector<UIElement *> handElements;
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
    static constexpr int slotCapacity = 3;
    static constexpr int slotCount = 3;

    UIManager(const char *mahjongSpritePath, int tilesPerRow, int tileWidth, int tileHeight)
        : muim(MahjongUIManager(mahjongSpritePath, tilesPerRow, tileWidth, tileHeight)) {}
    ~UIManager() = default;
    void cleanup();

    void update(Inventory &playerInventory);
    void draw(UpdateContext &uc, Inventory &playerInventory);
    void addElement(UIElement *element);
    void setSlotCooldownPercent(int slotIndex, float percent);

    void setPauseMenuVisible(bool visible);
    bool isPauseMenuVisible() const { return pauseMenuVisible; }
    bool consumeResumeRequest();
    bool consumeQuitRequest();
    const std::vector<SlotTileEntry> &getSlotEntries(int slotIndex) const;

    void setRewardBriefcaseUIOpen(bool open) { this->briefcaseUIOpen = open; }
    bool isRewardBriefcaseUIOpen() const { return this->briefcaseUIOpen; }
    void setHoveredTileIndex(int index) { this->hoveredTileIndex = index; }
    int getHoveredTileIndex() const { return this->hoveredTileIndex; }

    // Game Over UI
    void setGameOverVisible(bool visible) { this->gameOverVisible = visible; }
    bool isGameOverVisible() const { return this->gameOverVisible; }
    bool consumeRespawnRequest();
    void updateGameOverUI();
    void drawGameOverUI();

    // Briefcase menu: update/draw pair, mirrors pause menu design

    // Selection state for briefcase-hand swap
    int selectedBriefcaseIndex = -1;

    MahjongUIManager muim;

    void updateBriefcaseMenu(UpdateContext &uc, Inventory &playerInventory, bool& gamePaused);
    void drawBriefcaseMenu(UpdateContext &uc, Inventory &playerInventory);
    bool isTileFromHandUsed(int handIndex) const;

private:
    void updateHud();
    void drawHud();
    void drawSlotHudPreview();
    void updatePauseMenu(Inventory &playerInventory);
    void drawPauseMenu(Inventory &playerInventory);

    Rectangle getSmallButtonRect(int index) const;
    void drawSmallButton(const Rectangle &rect, const char *label, bool hovered) const;

    Rectangle getSlotRect(int index) const;
    void drawDraggingTile();
    void ensureSlotSetup();         // Initializes slot data from the hand on first use
    void ensureSlotElements();      // Lazily instantiates AttackSlotElement widgets
    void updateSlotElementLayout(); // Recomputes bounds + bindings every frame
    AttackSlotElement *getSlotElement(int index) const;

    void beginTileDragFromHand(int tileIndex, TileType tileType, const Vector2 &mousePos);
    void beginTileDragFromSlot(int slotIndex, int tileIndex, const Vector2 &mousePos);
    void endTileDrag(const Vector2 &mousePos);
    void addTileToSlot(int slotIndex, const SlotTileEntry &entry);

    bool slotHasSpace(int slotIndex) const;
    bool isValidSlotIndex(int slotIndex) const;

    // Rebuild briefcase UI element rectangles from inventory
    void rebuildBriefcaseUI(Inventory *briefcaseInventory);

    // Draw tile stats tooltip at top-right corner
    void drawTileStatsTooltip(const TileStats &stats, TileType type) const;

    std::vector<UIElement *> elements;
    std::vector<BriefcaseTileUI> briefcaseTileRects;
    bool pauseMenuVisible = false;
    bool resumeRequested = false;
    bool quitRequested = false;
    bool briefcaseUIOpen = false;
    bool gameOverVisible = false;
    bool respawnRequested = false;
    int activeBriefcaseIndex = -1; // index into Scene briefcases when menu is open
    int hoveredTileIndex = -1;
    // Hover states for briefcase UI
    int hoveredBriefcaseIndex = -1;
    int hoveredHandIndex = -1;
    std::array<std::vector<SlotTileEntry>, slotCount> attackSlots;
    std::array<AttackSlotElement *, slotCount> slotElements{}; // Owned UI wrappers for each slot
    std::array<float, slotCount> slotCooldowns{};
    static const char *slotKeyLabels[slotCount];
    bool slotsInitialized = false;
    bool isDraggingTile = false;
    bool draggingFromHand = false;
    int draggingFromSlot = -1;
    int draggingFromTileIndex = -1;
    SlotTileEntry draggingTile;
    Vector2 draggingTilePos{0.0f, 0.0f};
};
