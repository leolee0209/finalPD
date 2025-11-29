#include "uiManager.hpp"
#include "attackSlotElement.hpp"
#include <algorithm>
#include <cmath>

const char *UIManager::slotKeyLabels[slotCount] = {"Left Click", "Right Click", "E"};

void UIManager::cleanup()
{
    for (auto &element : elements)
    {
        delete element;
    }
    elements.clear();

    for (auto &slotElement : slotElements)
    {
        delete slotElement;
        slotElement = nullptr;
    }

    this->muim.cleanup();
}

void UIManager::addElement(UIElement *element)
{
    this->elements.push_back(element);
}

void UIManager::setSlotCooldownPercent(int slotIndex, float percent)
{
    if (!isValidSlotIndex(slotIndex))
        return;
    slotCooldowns[slotIndex] = std::clamp(percent, 0.0f, 1.0f);
}

void UIManager::setPauseMenuVisible(bool visible)
{
    pauseMenuVisible = visible;
    if (visible)
    {
        ensureSlotSetup();
    }
    if (!visible)
    {
        resumeRequested = false;
        quitRequested = false;
        isDraggingTile = false;
        draggingFromHand = false;
        draggingFromSlot = -1;
        draggingFromTileIndex = -1;
        draggingTile = SlotTileEntry{};
    }
}

bool UIManager::consumeResumeRequest()
{
    if (!resumeRequested)
        return false;
    resumeRequested = false;
    return true;
}

bool UIManager::consumeQuitRequest()
{
    if (!quitRequested)
        return false;
    quitRequested = false;
    return true;
}

void UIManager::update()
{
    if (pauseMenuVisible)
    {
        updatePauseMenu();
    }
    else
    {
        updateHud();
    }
}

void UIManager::draw()
{
    if (pauseMenuVisible)
    {
        drawPauseMenu();
    }
    else
    {
        drawHud();
    }
}

void UIManager::updateHud()
{
    for (auto &element : elements)
    {
        element->update();
    }
}

void UIManager::drawHud()
{
    for (auto &element : elements)
    {
        element->draw();
    }
    drawSlotHudPreview();
}

void UIManager::drawSlotHudPreview()
{
    ensureSlotSetup();

    const float slotWidth = 120.0f;
    const float slotHeight = 80.0f;
    const float spacing = 18.0f;
    const float tilePadding = 8.0f;
    const float tileSpacing = 10.0f;
    float totalHeight = slotCount * slotHeight + (slotCount - 1) * spacing;
    float startX = GetScreenWidth() - slotWidth - 28.0f;
    float startY = (GetScreenHeight() - totalHeight) * 0.5f;

    for (int i = 0; i < slotCount; ++i)
    {
        Rectangle slotRect{startX, startY + i * (slotHeight + spacing), slotWidth, slotHeight};
        Color frame = Color{8, 10, 16, 160};
        DrawRectangleRounded(slotRect, 0.26f, 6, frame);
        DrawRectangleRoundedLines(slotRect, 0.26f, 6, Fade(RAYWHITE, 1.0f));

        const char *label = slotKeyLabels[i];
        if (label && *label)
        {
            const int labelFont = 16;
            int textWidth = MeasureText(label, labelFont);
            int textX = static_cast<int>(slotRect.x + slotRect.width / 2.0f - textWidth / 2.0f);
            int textY = static_cast<int>(slotRect.y - labelFont - 2.0f);
            DrawText(label, textX, textY, labelFont, Fade(RAYWHITE, 0.55f));
        }

        DrawRectangleRounded(slotRect, 0.26f, 6, Fade(BLACK, 0.1f));

        const auto &slot = attackSlots[i];
        const int tileCount = static_cast<int>(slot.size());

        float usableWidth = slotRect.width - tilePadding * 2.0f;
        float cellWidth = (usableWidth - tileSpacing * (slotCapacity - 1)) / slotCapacity;
        float cellHeight = slotRect.height - tilePadding * 2.0f;
        for (int tileIdx = 0; tileIdx < slotCapacity; ++tileIdx)
        {
            float tileX = slotRect.x + tilePadding + tileIdx * (cellWidth + tileSpacing);
            float tileY = slotRect.y + tilePadding;
            Rectangle tileRect{tileX, tileY, cellWidth, cellHeight};
            DrawRectangleRounded(tileRect, 0.18f, 4, Fade(WHITE, 0.05f));
            DrawRectangleRoundedLines(tileRect, 0.18f, 4, Fade(RAYWHITE, 0.15f));

            if (tileIdx >= tileCount)
                continue;

            const SlotTileEntry &entry = slot[tileIdx];
            if (!entry.isValid())
                continue;

            Rectangle source = muim.getTile(entry.tile);
            float scale = fminf(tileRect.width / source.width, tileRect.height / source.height) * 0.9f;
            Vector2 spriteSize{source.width * scale, source.height * scale};
            Rectangle dest{tileRect.x + tileRect.width * 0.5f - spriteSize.x * 0.5f,
                           tileRect.y + tileRect.height * 0.5f - spriteSize.y * 0.5f,
                           spriteSize.x,
                           spriteSize.y};
            DrawTexturePro(muim.getSpriteSheet(), source, dest, {0.0f, 0.0f}, 0.0f, Fade(WHITE, 0.55f));
        }

        float cooldown = slotCooldowns[i];
        if (cooldown > 0.001f)
        {
            Rectangle cooldownRect = slotRect;
            cooldownRect.width = slotRect.width * std::clamp(cooldown, 0.0f, 1.0f);
            DrawRectangleRounded(cooldownRect, 0.18f, 4, Color{120, 120, 120, 160});
        }
    }
}

void UIManager::updatePauseMenu()
{
    ensureSlotSetup();
    ensureSlotElements();
    this->muim.update();
    updateSlotElementLayout();

    Rectangle resumeRect = getSmallButtonRect(0);
    Rectangle quitRect = getSmallButtonRect(1);
    Vector2 mousePos = GetMousePosition();
    bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool mouseReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    if (mousePressed)
    {
        if (CheckCollisionPointRec(mousePos, resumeRect))
        {
            resumeRequested = true;
        }
        else if (CheckCollisionPointRec(mousePos, quitRect))
        {
            quitRequested = true;
        }
    }

    if (!isDraggingTile && mousePressed)
    {
        int tileIndex = muim.getTileIndexAt(mousePos);
        if (tileIndex >= 0)
        {
            beginTileDragFromHand(tileIndex, mousePos);
        }
        else
        {
            bool grabbedSlotTile = false;
            const int slotCountLocal = static_cast<int>(attackSlots.size());
            for (int slotIdx = 0; slotIdx < slotCountLocal && !grabbedSlotTile; ++slotIdx)
            {
                const auto &slot = attackSlots[slotIdx];
                AttackSlotElement *slotElement = getSlotElement(slotIdx);
                if (!slotElement)
                    continue;
                for (int tileSlot = 0; tileSlot < static_cast<int>(slot.size()); ++tileSlot)
                {
                    if (CheckCollisionPointRec(mousePos, slotElement->getTileRect(tileSlot)))
                    {
                        beginTileDragFromSlot(slotIdx, tileSlot, mousePos);
                        grabbedSlotTile = true;
                        break;
                    }
                }
            }

        }
    }

    if (isDraggingTile)
    {
        draggingTilePos = mousePos;
        if (mouseReleased)
        {
            endTileDrag(mousePos);
        }
    }
}

void UIManager::drawPauseMenu()
{
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.65f));

    Rectangle resumeRect = getSmallButtonRect(0);
    Rectangle quitRect = getSmallButtonRect(1);
    Vector2 mousePos = GetMousePosition();

    drawSmallButton(resumeRect, "Resume", CheckCollisionPointRec(mousePos, resumeRect));
    drawSmallButton(quitRect, "Quit", CheckCollisionPointRec(mousePos, quitRect));

    const int slotCountLocal = static_cast<int>(attackSlots.size());
    for (int i = 0; i < slotCountLocal; ++i)
    {
        if (AttackSlotElement *slotElement = getSlotElement(i))
        {
            slotElement->draw();
        }
    }

    this->muim.draw();
    if (isDraggingTile)
    {
        drawDraggingTile();
    }
}

Rectangle UIManager::getSmallButtonRect(int index) const
{
    const float width = 140.0f;
    const float height = 36.0f;
    const float spacing = 12.0f;
    float startX = 20.0f;
    float startY = 70.0f + index * (height + spacing);
    return {startX, startY, width, height};
}

void UIManager::drawSmallButton(const Rectangle &rect, const char *label, bool hovered) const
{
    Color baseColor = hovered ? Color{70, 120, 200, 220} : Color{40, 60, 90, 220};
    DrawRectangleRounded(rect, 0.2f, 8, baseColor);
    DrawRectangleRoundedLines(rect, 0.2f, 8, Fade(RAYWHITE, 0.9f));

    const int fontSize = 24;
    int textWidth = MeasureText(label, fontSize);
    int textX = static_cast<int>(rect.x + rect.width / 2.0f - textWidth / 2.0f);
    int textY = static_cast<int>(rect.y + rect.height / 2.0f - fontSize / 2.0f);
    DrawText(label, textX, textY, fontSize, RAYWHITE);
}

Rectangle UIManager::getSlotRect(int index) const
{
    const float slotWidth = 140.0f;
    const float slotHeight = 80.0f;
    const float spacing = 32.0f;
    float slotCountF = static_cast<float>(attackSlots.size());
    float totalHeight = slotHeight * slotCountF + spacing * (slotCountF - 1.0f);
    float startX = (GetScreenWidth() - slotWidth) * 0.5f;
    float startY = (GetScreenHeight() - totalHeight) * 0.45f;
    return {startX, startY + index * (slotHeight + spacing), slotWidth, slotHeight};
}

void UIManager::drawDraggingTile()
{
    if (draggingTile.tile == MahjongTileType::EMPTY)
        return;
    Rectangle source = muim.getTile(draggingTile.tile);
    float scale = 0.9f;
    Rectangle dest{draggingTilePos.x - source.width * scale * 0.5f, draggingTilePos.y - source.height * scale * 0.5f, source.width * scale, source.height * scale};
    DrawTexturePro(muim.getSpriteSheet(), source, dest, {0.0f, 0.0f}, 0.0f, WHITE);
}

void UIManager::ensureSlotSetup()
{
    if (slotsInitialized)
        return;

    const auto &hand = muim.getPlayerHand();
    for (auto &slot : attackSlots)
    {
        slot.clear();
        slot.reserve(slotCapacity);
    }

    slotsInitialized = true;
}

void UIManager::ensureSlotElements()
{
    // Lazily create the three UI widgets so we do not pay for them until the
    // pause menu opens. Each element is persistent because it keeps its own
    // animation state and allows UIManager to drive only the gameplay data.
    for (int i = 0; i < slotCount; ++i)
    {
        if (!slotElements[i])
        {
            slotElements[i] = new AttackSlotElement(i, slotCapacity, muim);
            slotElements[i]->setKeyLabel(slotKeyLabels[i]);
        }
    }
}

void UIManager::updateSlotElementLayout()
{
    // Layout depends on the current screen resolution, so recompute each frame
    // before drawing/handling input.
    const int slotCountLocal = static_cast<int>(attackSlots.size());
    for (int i = 0; i < slotCountLocal; ++i)
    {
        AttackSlotElement *slotElement = getSlotElement(i);
        if (!slotElement)
            continue;

        slotElement->setBounds(getSlotRect(i));
        slotElement->setEntries(&attackSlots[i]);
        slotElement->setActive(false);
    }
}

AttackSlotElement *UIManager::getSlotElement(int index) const
{
    if (index < 0 || index >= slotCount)
        return nullptr;
    return slotElements[index];
}

void UIManager::beginTileDragFromHand(int tileIndex, const Vector2 &mousePos)
{
    const auto &hand = muim.getPlayerHand();
    if (tileIndex < 0 || tileIndex >= static_cast<int>(hand.size()))
        return;
    if (muim.isTileUsed(tileIndex))
        return;

    draggingTile = SlotTileEntry{hand[tileIndex], tileIndex};
    draggingTilePos = mousePos;
    isDraggingTile = true;
    draggingFromHand = true;
    draggingFromSlot = -1;
    draggingFromTileIndex = -1;
}

void UIManager::beginTileDragFromSlot(int slotIndex, int tileIndex, const Vector2 &mousePos)
{
    if (!isValidSlotIndex(slotIndex))
        return;
    auto &slot = attackSlots[slotIndex];
    if (tileIndex < 0 || tileIndex >= static_cast<int>(slot.size()))
        return;

    draggingTile = slot[tileIndex];
    muim.setTileUsed(draggingTile.handIndex, false);
    slot.erase(slot.begin() + tileIndex);
    draggingTilePos = mousePos;
    isDraggingTile = true;
    draggingFromHand = false;
    draggingFromSlot = slotIndex;
    draggingFromTileIndex = tileIndex;

}

void UIManager::endTileDrag(const Vector2 &mousePos)
{
    bool placed = false;
    for (int i = 0; i < slotCount; ++i)
    {
        AttackSlotElement *slotElement = getSlotElement(i);
        if (slotElement && slotElement->containsPoint(mousePos) && slotHasSpace(i))
        {
            addTileToSlot(i, draggingTile);
            placed = true;
            break;
        }
    }

    bool removedFromSlot = false;
    if (!placed && !draggingFromHand)
    {
        // Dropping outside clears the slot entry so the tile becomes available
        // in the hand again. We no longer snap the piece back to its slot.
        removedFromSlot = true;
    }

    isDraggingTile = false;
    draggingTile = SlotTileEntry{};
    draggingFromHand = false;
    draggingFromSlot = -1;
    draggingFromTileIndex = -1;

}

void UIManager::addTileToSlot(int slotIndex, const SlotTileEntry &entry)
{
    if (!isValidSlotIndex(slotIndex) || entry.tile == MahjongTileType::EMPTY || entry.handIndex < 0)
        return;

    auto &slot = attackSlots[slotIndex];
    if (static_cast<int>(slot.size()) >= slotCapacity)
        return;

    slot.push_back(entry);
    muim.setTileUsed(entry.handIndex, true);
}

bool UIManager::slotHasSpace(int slotIndex) const
{
    if (!isValidSlotIndex(slotIndex))
        return false;
    return static_cast<int>(attackSlots[slotIndex].size()) < slotCapacity;
}

const std::vector<SlotTileEntry> &UIManager::getSlotEntries(int slotIndex) const
{
    static const std::vector<SlotTileEntry> empty;
    if (!isValidSlotIndex(slotIndex))
        return empty;
    return attackSlots[slotIndex];
}

bool UIManager::isValidSlotIndex(int slotIndex) const
{
    return slotIndex >= 0 && slotIndex < slotCount;
}


Texture2D UIManager::loadTexture(const std::string &fileName)
{
    return LoadTexture(fileName.c_str());
}