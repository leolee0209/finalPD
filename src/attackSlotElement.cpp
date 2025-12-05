#include "attackSlotElement.hpp"
#include "uiManager.hpp"
#include <cmath>

AttackSlotElement::AttackSlotElement(int slotIdx, int slotCapacity, MahjongUIManager &ui)
    : mahjongUI(ui), slotIndex(slotIdx), capacity(slotCapacity)
{
    position = {0.0f, 0.0f};
    size = {140.0f, 80.0f};
}

void AttackSlotElement::setEntries(const std::vector<SlotTileEntry> *entriesRef)
{
    entries = entriesRef;
}

void AttackSlotElement::setKeyLabel(const std::string &label)
{
    keyLabel = label;
}

void AttackSlotElement::setBounds(const Rectangle &rect)
{
    position = {rect.x, rect.y};
    size = {rect.width, rect.height};
}

Rectangle AttackSlotElement::getTileRect(int tileIndex) const
{
    // Tiles are laid out horizontally with constant padding so gameplay code
    // can keep referring to index-based rectangles even as screen size changes.
    float availableWidth = size.x - padding * 2.0f;
    float cellWidth = (availableWidth - spacing * (capacity - 1)) / capacity;
    float cellHeight = size.y - padding * 2.0f;
    float x = position.x + padding + tileIndex * (cellWidth + spacing);
    float y = position.y + padding;
    return {x, y, cellWidth, cellHeight};
}

bool AttackSlotElement::containsPoint(const Vector2 &point) const
{
    return CheckCollisionPointRec(point, getBounds());
}

void AttackSlotElement::draw()
{
    // Frame + label ---------------------------------------------------------
    Rectangle rect = getBounds();
    Color base = Color{30, 35, 45, 230};
    DrawRectangleRounded(rect, 0.18f, 8, base);
    Color outline = isValidCombo ? Fade(RAYWHITE, 0.9f) : RED;
    float thick = isValidCombo ? 1.0f : 3.0f;
    DrawRectangleRoundedLinesEx(rect, 0.18f, 8, thick, outline);

    if (!keyLabel.empty())
    {
        const int labelFont = 20;
        int labelWidth = MeasureText(keyLabel.c_str(), labelFont);
        DrawText(keyLabel.c_str(), rect.x + rect.width / 2 - labelWidth / 2, rect.y - labelFont - 4, labelFont, RAYWHITE);
    }

    // Slots mirror the gameplay data vector; empty cells render dark pads so
    // players always understand capacity.
    const int tileCount = entries ? static_cast<int>(entries->size()) : 0;
    for (int i = 0; i < capacity; ++i)
    {
        Rectangle tileRect = getTileRect(i);
        DrawRectangleRounded(tileRect, 0.2f, 4, Color{10, 15, 25, 200});
        DrawRectangleRoundedLines(tileRect, 0.2f, 4, Fade(RAYWHITE, 0.3f));

        if (i >= tileCount)
        {
            continue;
        }

        // Entry pointers stay valid for the frame (UIManager owns storage).
        const SlotTileEntry &entry = (*entries)[i];
        if (!entry.isValid())
            continue;

        Rectangle source = mahjongUI.getTile(entry.tile);
        float scale = fminf(tileRect.width / source.width, tileRect.height / source.height) * 0.9f;
        Vector2 spriteSize{source.width * scale, source.height * scale};
        Rectangle dest{tileRect.x + tileRect.width / 2 - spriteSize.x / 2,
                       tileRect.y + tileRect.height / 2 - spriteSize.y / 2,
                       spriteSize.x,
                       spriteSize.y};
        DrawTexturePro(mahjongUI.getSpriteSheet(), source, dest, {0.0f, 0.0f}, 0.0f, WHITE);
    }
}
