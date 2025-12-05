#pragma once

#include "uiElement.hpp"
#include "tiles.hpp"
#include <string>
#include <vector>

class MahjongUIManager;

/**
 * @brief Visual + hit-test wrapper for a single attack slot in the pause menu.
 *
 * UIManager keeps the gameplay data (which tiles are assigned) and feeds the
 * current SlotTileEntry vector into this widget so rendering/interaction stay
 * encapsulated. That keeps slot math (layout, labels, hover rects) out of the
 * manager and makes the slot reusable for other screens if needed.
 */
class AttackSlotElement : public UIElement
{
public:
    AttackSlotElement(int slotIndex, int capacity, MahjongUIManager &mahjongUI);
    ~AttackSlotElement() override = default;

    void setEntries(const std::vector<SlotTileEntry> *entries); // Pointer stays owned by UIManager
    void setValid(bool valid) { isValidCombo = valid; }
    void setKeyLabel(const std::string &label);                 // Displays "1/2/3" labels above the frame
    void setBounds(const Rectangle &rect);                      // UIManager drives layout each frame
    Rectangle getTileRect(int tileIndex) const;                 // Used for drag/drop hit testing
    bool containsPoint(const Vector2 &point) const;

    void draw() override;
    void update() override {}

private:
    const std::vector<SlotTileEntry> *entries = nullptr;
    MahjongUIManager &mahjongUI;
    int slotIndex = 0;
    int capacity = 0;
    bool isValidCombo = true;
    std::string keyLabel;
    float padding = 5.0f;
    float spacing = 10.0f;
};
