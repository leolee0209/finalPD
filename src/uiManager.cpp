#include "uiManager.hpp"
#include "attackSlotElement.hpp"
#include "rewardBriefcase.hpp"
#include "Inventory.hpp"
#include "scene.hpp"
#include <algorithm>
#include <cmath>

const char *UIManager::slotKeyLabels[slotCount] = {"Right Click", "R", "E"};

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

bool UIManager::consumeRespawnRequest()
{
    if (!respawnRequested)
        return false;
    respawnRequested = false;
    return true;
}

void UIManager::updateGameOverUI()
{
    if (!gameOverVisible)
        return;

    // Check for respawn button click
    Vector2 mousePos = GetMousePosition();
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    
    // Respawn button centered below "GAME OVER" text
    Rectangle respawnButton = {
        (float)screenW / 2.0f - 100.0f,
        (float)screenH / 2.0f + 50.0f,
        200.0f,
        50.0f
    };
    
    bool hovered = CheckCollisionPointRec(mousePos, respawnButton);
    
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        respawnRequested = true;
        gameOverVisible = false;
    }
}

void UIManager::drawGameOverUI()
{
    if (!gameOverVisible)
        return;
    
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    
    // Semi-transparent dark overlay
    DrawRectangle(0, 0, screenW, screenH, ColorAlpha(BLACK, 0.7f));
    
    // "GAME OVER" text
    const char *gameOverText = "GAME OVER";
    int fontSize = 80;
    int textWidth = MeasureText(gameOverText, fontSize);
    DrawText(gameOverText, screenW / 2 - textWidth / 2, screenH / 2 - 100, fontSize, RED);
    
    // Respawn button
    Rectangle respawnButton = {
        (float)screenW / 2.0f - 100.0f,
        (float)screenH / 2.0f + 50.0f,
        200.0f,
        50.0f
    };
    
    Vector2 mousePos = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mousePos, respawnButton);
    
    Color buttonColor = hovered ? DARKGREEN : GREEN;
    DrawRectangleRec(respawnButton, buttonColor);
    DrawRectangleLinesEx(respawnButton, 3, WHITE);
    
    const char *buttonText = "RESPAWN";
    int buttonTextSize = 30;
    int buttonTextWidth = MeasureText(buttonText, buttonTextSize);
    DrawText(buttonText, 
             (int)(respawnButton.x + respawnButton.width / 2 - buttonTextWidth / 2),
             (int)(respawnButton.y + respawnButton.height / 2 - buttonTextSize / 2),
             buttonTextSize, WHITE);
}

const std::vector<SlotTileEntry> &UIManager::getSlotEntries(int slotIndex) const
{
    static const std::vector<SlotTileEntry> empty;
    if (!isValidSlotIndex(slotIndex))
        return empty;
    return attackSlots[slotIndex];
}

void UIManager::update(Inventory &playerInventory)
{
    // Update game over UI first (highest priority)
    if (gameOverVisible)
    {
        updateGameOverUI();
        return; // Don't process other UI when game over is shown
    }
    
    // Suppress ESC pause toggle while briefcase UI is open
    if (this->briefcaseUIOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            // ignore
        }
    }
    
    // Create/update hand UI from player inventory
    muim.createHandUI(playerInventory, GetScreenWidth(), GetScreenHeight());
    
    if (pauseMenuVisible)
    {
        updatePauseMenu(playerInventory);
    }
    else
    {
        updateHud();
    }
}

void UIManager::draw(UpdateContext& uc, Inventory &playerInventory)
{
    if (gameOverVisible)
    {
        drawGameOverUI();
        return; // Game over UI takes full screen
    }
    
    if (pauseMenuVisible)
    {
        drawPauseMenu(playerInventory);
    }
    else
    {
        drawHud();
    }
    // Draw briefcase menu if active
    if (this->briefcaseUIOpen)
    {
        drawBriefcaseMenu(uc, playerInventory);
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

void UIManager::rebuildBriefcaseUI(Inventory *briefcaseInventory)
{
    briefcaseTileRects.clear();
    if (!briefcaseInventory)
        return;

    const auto &btiles = briefcaseInventory->getTiles();
    const int count = (int)btiles.size();
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float tileW = 44.0f * 1.2f;
    float tileH = 60.0f * 1.2f;
    float spacing = 8.0f;
    float totalW = count * tileW + (count - 1) * spacing;
    float startX = (screenW - totalW) * 0.5f;
    float startY = screenH * 0.18f;

    for (int i = 0; i < count; ++i)
    {
        BriefcaseTileUI ui;
        ui.rect = Rectangle{startX + i * (tileW + spacing), startY, tileW, tileH};
        ui.tileType = btiles[i].type;
        ui.inventoryIndex = i;
        briefcaseTileRects.push_back(ui);
    }
}

void UIManager::drawTileStatsTooltip(const TileStats &stats, TileType type) const
{
    const int screenW = GetScreenWidth();
    const int padding = 20;
    const int boxWidth = 180;
    const int boxHeight = 80;
    const int fontSize = 18;
    const int titleFont = 20;
    
    Rectangle tooltipRect = {
        (float)(screenW - boxWidth - padding),
        (float)padding,
        (float)boxWidth,
        (float)boxHeight
    };
    
    // Draw background
    DrawRectangleRounded(tooltipRect, 0.15f, 8, Color{20, 25, 35, 240});
    DrawRectangleRoundedLines(tooltipRect, 0.15f, 8, Fade(RAYWHITE, 0.6f));
    
    // Draw title
    const char *title = "Tile Stats";
    int titleWidth = MeasureText(title, titleFont);
    DrawText(title, 
        (int)(tooltipRect.x + tooltipRect.width * 0.5f - titleWidth * 0.5f),
        (int)(tooltipRect.y + 10),
        titleFont,
        YELLOW);
    
    // Draw damage
    char damageText[64];
    snprintf(damageText, sizeof(damageText), "Damage: %.1f", stats.damage);
    DrawText(damageText,
        (int)(tooltipRect.x + 15),
        (int)(tooltipRect.y + 38),
        fontSize,
        RAYWHITE);
    
    // Draw fire rate
    char fireRateText[64];
    snprintf(fireRateText, sizeof(fireRateText), "Fire Rate: %.2fx", stats.fireRate);
    DrawText(fireRateText,
        (int)(tooltipRect.x + 15),
        (int)(tooltipRect.y + 56),
        fontSize,
        RAYWHITE);
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

Rectangle UIManager::getSmallButtonRect(int index) const
{
    const float buttonWidth = 100.0f;
    const float buttonHeight = 42.0f;
    const float spacing = 14.0f;
    float startX = (GetScreenWidth() - buttonWidth) * 0.1f;
    float startY = GetScreenHeight() * 0.1f;
    return {startX, startY + index * (buttonHeight + spacing), buttonWidth, buttonHeight};
}

void UIManager::drawSmallButton(const Rectangle &rect, const char *label, bool hovered) const
{
    Color fill = hovered ? Color{70, 120, 160, 220} : Color{30, 35, 45, 220};
    DrawRectangleRounded(rect, 0.22f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.22f, 8, Fade(RAYWHITE, 0.9f));
    int font = 22;
    int tw = MeasureText(label, font);
    DrawText(label, (int)(rect.x + rect.width * 0.5f - tw * 0.5f), (int)(rect.y + rect.height * 0.5f - font * 0.5f), font, RAYWHITE);
}

Rectangle UIManager::getSlotRect(int index) const
{
    const float slotWidth = 250.0f;
    const float slotHeight = 88.0f;
    const float spacing = 30.0f;
    float startX = (GetScreenWidth() - slotWidth) * 0.5f;
    float centerY = GetScreenHeight() * 0.45f;
    float totalHeight = slotCount * slotHeight + (slotCount - 1) * spacing;
    float startY = centerY - totalHeight * 0.5f;
    return {startX, startY + index * (slotHeight + spacing), slotWidth, slotHeight};
}

void UIManager::drawDraggingTile()
{
    if (!isDraggingTile || draggingTile.tile == TileType::EMPTY)
        return;

    Rectangle source = muim.getTile(draggingTile.tile);
    float width = 72.0f;
    float height = width * (source.height / source.width);
    Rectangle dest{draggingTilePos.x - width * 0.5f, draggingTilePos.y - height * 0.5f, width, height};
    DrawTexturePro(muim.getSpriteSheet(), source, dest, {0.0f, 0.0f}, 0.0f, WHITE);
}

void UIManager::ensureSlotSetup()
{
    if (slotsInitialized)
        return;
    for (auto &v : attackSlots)
        v.clear();
    for (auto &c : slotCooldowns)
        c = 0.0f;
    slotsInitialized = true;
}

void UIManager::ensureSlotElements()
{
    for (int i = 0; i < slotCount; ++i)
    {
        if (!slotElements[i])
        {
            slotElements[i] = new AttackSlotElement(i, slotCapacity, muim);
            slotElements[i]->setEntries(&attackSlots[i]);
            slotElements[i]->setKeyLabel(slotKeyLabels[i]);
        }
        slotElements[i]->setBounds(getSlotRect(i));
    }
}

void UIManager::updateSlotElementLayout()
{
    ensureSlotElements();
    for (int i = 0; i < slotCount; ++i)
    {
        slotElements[i]->setBounds(getSlotRect(i));
    }
}

AttackSlotElement *UIManager::getSlotElement(int index) const
{
    if (!isValidSlotIndex(index))
        return nullptr;
    return slotElements[index];
}

void UIManager::beginTileDragFromHand(int tileIndex, TileType tileType, const Vector2 &mousePos)
{
    isDraggingTile = true;
    draggingFromHand = true;
    draggingFromSlot = -1;
    draggingFromTileIndex = tileIndex;
    draggingTile = SlotTileEntry{tileType, tileIndex};
    draggingTilePos = mousePos;
}

void UIManager::beginTileDragFromSlot(int slotIndex, int tileIndex, const Vector2 &mousePos)
{
    if (!isValidSlotIndex(slotIndex))
        return;
    if (tileIndex < 0 || tileIndex >= (int)attackSlots[slotIndex].size())
        return;
    isDraggingTile = true;
    draggingFromHand = false;
    draggingFromSlot = slotIndex;
    draggingFromTileIndex = tileIndex;
    draggingTile = attackSlots[slotIndex][tileIndex];
    // Remove from slot while dragging
    attackSlots[slotIndex].erase(attackSlots[slotIndex].begin() + tileIndex);
    draggingTilePos = mousePos;
}

void UIManager::endTileDrag(const Vector2 &mousePos)
{
    if (!isDraggingTile)
        return;

    int targetSlot = -1;
    for (int i = 0; i < slotCount; ++i)
    {
        if (CheckCollisionPointRec(mousePos, getSlotRect(i)))
        {
            targetSlot = i;
            break;
        }
    }

    if (targetSlot >= 0 && slotHasSpace(targetSlot))
    {
        addTileToSlot(targetSlot, draggingTile);
    }
    else if (!draggingFromHand && draggingFromSlot >= 0)
    {
        // If dragged from slot and dropped outside all slots, remove the tile (don't return it)
        // This allows players to remove tiles by dragging them out
        // Only return to original slot if dropped on a full slot
        if (targetSlot >= 0)
        {
            // Tried to drop on a slot but it was full
            addTileToSlot(draggingFromSlot, draggingTile);
        }
        // Otherwise, tile is removed (not added back)
    }

    isDraggingTile = false;
    draggingFromHand = false;
    draggingFromSlot = -1;
    draggingFromTileIndex = -1;
    draggingTile = SlotTileEntry{};
}

void UIManager::addTileToSlot(int slotIndex, const SlotTileEntry &entry)
{
    if (!isValidSlotIndex(slotIndex))
        return;
    auto &slot = attackSlots[slotIndex];
    if ((int)slot.size() < slotCapacity)
    {
        // Check if this hand tile is already used in any slot
        if (entry.handIndex >= 0)
        {
            for (int s = 0; s < slotCount; ++s)
            {
                for (const auto &existing : attackSlots[s])
                {
                    if (existing.handIndex == entry.handIndex)
                    {
                        return; // Don't allow duplicate
                    }
                }
            }
        }
        slot.push_back(entry);
    }
}

bool UIManager::slotHasSpace(int slotIndex) const
{
    if (!isValidSlotIndex(slotIndex))
        return false;
    return (int)attackSlots[slotIndex].size() < slotCapacity;
}

bool UIManager::isValidSlotIndex(int slotIndex) const
{
    return slotIndex >= 0 && slotIndex < slotCount;
}

bool UIManager::isTileFromHandUsed(int handIndex) const
{
    for (int s = 0; s < slotCount; ++s)
    {
        for (const auto &entry : attackSlots[s])
        {
            if (entry.handIndex == handIndex)
            {
                return true;
            }
        }
    }
    return false;
}

void UIManager::updatePauseMenu(Inventory &playerInventory)
{
    ensureSlotSetup();
    updateSlotElementLayout();
    muim.update(playerInventory);

    Vector2 mouse = GetMousePosition();
    draggingTilePos = mouse;

    // Hover hand tile
    hoveredTileIndex = muim.getTileIndexAt(mouse);

    // Handle left click on hand tiles - for selection
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoveredTileIndex >= 0)
    {
        auto &tiles = playerInventory.getTiles();
        if (hoveredTileIndex < (int)tiles.size())
        {
            // Select this tile as the basic attack tile
            muim.selectTileByIndex(hoveredTileIndex);
        }
    }

    // Handle right click on hand tiles - for dragging to slots
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hoveredTileIndex >= 0)
    {
        auto &tiles = playerInventory.getTiles();
        if (hoveredTileIndex < (int)tiles.size())
        {
            beginTileDragFromHand(hoveredTileIndex, tiles[hoveredTileIndex].type, mouse);
        }
    }

    // Start drag from slot with right-click
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hoveredTileIndex < 0)
    {
        for (int s = 0; s < slotCount; ++s)
        {
            AttackSlotElement *slotEl = getSlotElement(s);
            if (!slotEl)
                continue;
            if (!slotEl->containsPoint(mouse))
                continue;
            // Check tile cells
            int tileIdx = -1;
            int tileCount = (int)attackSlots[s].size();
            for (int i = 0; i < tileCount; ++i)
            {
                if (CheckCollisionPointRec(mouse, slotEl->getTileRect(i)))
                {
                    tileIdx = i;
                    break;
                }
            }
            if (tileIdx >= 0)
            {
                beginTileDragFromSlot(s, tileIdx, mouse);
                break;
            }
        }
    }

    if (isDraggingTile)
    {
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
        {
            endTileDrag(mouse);
        }
    }

    // Handle buttons
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (CheckCollisionPointRec(mouse, getSmallButtonRect(0)))
        {
            resumeRequested = true;
        }
        else if (CheckCollisionPointRec(mouse, getSmallButtonRect(1)))
        {
            quitRequested = true;
        }
    }
}

void UIManager::drawPauseMenu(Inventory &playerInventory)
{
    // Dim background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 140});

    // Buttons
    Vector2 mouse = GetMousePosition();
    drawSmallButton(getSmallButtonRect(0), "Resume", CheckCollisionPointRec(mouse, getSmallButtonRect(0)));
    drawSmallButton(getSmallButtonRect(1), "Quit", CheckCollisionPointRec(mouse, getSmallButtonRect(1)));

    // Hand
    muim.draw();
    
    // Gray out tiles that are already used in slots
    for (int i = 0; i < muim.getTileCount(); ++i)
    {
        if (isTileFromHandUsed(i))
        {
            Rectangle tileRect = muim.getTileRect(i);
            DrawRectangleRec(tileRect, Fade(GRAY, 0.6f));
        }
    };

    // Slots
    ensureSlotElements();
    for (int i = 0; i < slotCount; ++i)
    {
        slotElements[i]->draw();
    }

    // Drag preview
    drawDraggingTile();

    // Show tile stats tooltip if hovering over a hand tile
    if (hoveredTileIndex >= 0 && !isDraggingTile)
    {
        const auto &tiles = playerInventory.getTiles();
        if (hoveredTileIndex < (int)tiles.size())
        {
            drawTileStatsTooltip(tiles[hoveredTileIndex].stat, tiles[hoveredTileIndex].type);
        }
    }
}

void UIManager::updateBriefcaseMenu(UpdateContext &uc, Inventory &playerInventory, bool& gamePaused)
{
    // Build/update player hand elements
    muim.createHandUI(playerInventory, GetScreenWidth(), GetScreenHeight());
    muim.update(playerInventory);

    Vector2 mouse = GetMousePosition();
    hoveredHandIndex = muim.getTileIndexAt(mouse);

    // Query scene for briefcases
    auto briefcases = uc.scene ? uc.scene->GetRewardBriefcases() : std::vector<RewardBriefcase*>{};
    // If menu not open, check activation (nearby + C)
    if (!this->briefcaseUIOpen)
    {
        // Find nearby briefcase to activate
        int index = -1;
        for (int i = 0; i < (int)briefcases.size(); ++i)
        {
            RewardBriefcase *b = briefcases[i];
            if (!b) continue;
            if (b->IsPlayerNearby(uc.player->pos()))
            {
                index = i;
                break;
            }
        }
        if (index >= 0 && IsKeyPressed(KEY_C))
        {
            this->briefcaseUIOpen = true;
            this->activeBriefcaseIndex = index;
            if (briefcases[index]) briefcases[index]->SetActivated(true);
            EnableCursor();
            gamePaused = true;
        }
        return; // nothing else if menu closed
    }

    // Active briefcase inventory
    Inventory *briefInv = nullptr;
    if (this->activeBriefcaseIndex >= 0 && this->activeBriefcaseIndex < (int)briefcases.size())
    {
        if (briefcases[this->activeBriefcaseIndex])
        {
            briefInv = &briefcases[this->activeBriefcaseIndex]->GetInventory();
        }
    }
    if (!briefInv)
    {
        // No valid briefcase, close menu
        this->briefcaseUIOpen = false;
        this->activeBriefcaseIndex = -1;
        return;
    }

    // Rebuild briefcase UI elements from current inventory
    rebuildBriefcaseUI(briefInv);

    // Check hover state using cached rectangles
    hoveredBriefcaseIndex = -1;
    for (int i = 0; i < (int)briefcaseTileRects.size(); ++i)
    {
        if (CheckCollisionPointRec(mouse, briefcaseTileRects[i].rect))
        {
            hoveredBriefcaseIndex = i;
            break;
        }
    }

    // Swap logic: click briefcase tile then click hand tile to swap
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (hoveredBriefcaseIndex >= 0)
        {
            selectedBriefcaseIndex = hoveredBriefcaseIndex;
        }
        else if (selectedBriefcaseIndex >= 0 && hoveredHandIndex >= 0)
        {
            auto &p = playerInventory.getTiles();
            auto &b = const_cast<std::vector<Tile>&>(briefInv->getTiles());
            if (hoveredHandIndex < (int)p.size() && selectedBriefcaseIndex < (int)b.size())
            {
                std::swap(p[hoveredHandIndex], b[selectedBriefcaseIndex]);
            }
            selectedBriefcaseIndex = -1;
        }
    }

    // Close with C or ESC
    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_ESCAPE))
    {
        this->briefcaseUIOpen = false;
        if (this->activeBriefcaseIndex >= 0 && this->activeBriefcaseIndex < (int)briefcases.size() && briefcases[this->activeBriefcaseIndex])
        {
            briefcases[this->activeBriefcaseIndex]->SetActivated(false);
            DisableCursor();
            gamePaused = false;
        }
        this->activeBriefcaseIndex = -1;
    }
}

void UIManager::drawBriefcaseMenu(UpdateContext &uc, Inventory &playerInventory)
{
    // Dim background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 140});

    // Resolve active briefcase inventory
    Inventory *briefInv = nullptr;
    auto briefcases = uc.scene ? uc.scene->GetRewardBriefcases() : std::vector<RewardBriefcase*>{};
    if (this->activeBriefcaseIndex >= 0 && this->activeBriefcaseIndex < (int)briefcases.size() && briefcases[this->activeBriefcaseIndex])
    {
        briefInv = &briefcases[this->activeBriefcaseIndex]->GetInventory();
    }
    if (!briefInv)
    {
        return;
    }

    // Draw briefcase tiles using cached UI elements
    for (int i = 0; i < (int)briefcaseTileRects.size(); ++i)
    {
        const auto &ui = briefcaseTileRects[i];
        DrawRectangleRounded(ui.rect, 0.12f, 6, Color{20, 20, 28, 220});
        Rectangle src = muim.getTile(ui.tileType);
        float scale = fminf(ui.rect.width / src.width, ui.rect.height / src.height) * 0.92f;
        Vector2 size{src.width * scale, src.height * scale};
        Rectangle dest{ui.rect.x + ui.rect.width * 0.5f - size.x * 0.5f,
                       ui.rect.y + ui.rect.height * 0.5f - size.y * 0.5f,
                       size.x, size.y};
        DrawTexturePro(muim.getSpriteSheet(), src, dest, {0, 0}, 0.0f, WHITE);
        if (i == selectedBriefcaseIndex)
        {
            DrawRectangleLinesEx(ui.rect, 2.0f, YELLOW);
        }
    }

    // Draw player hand UI at bottom
    muim.draw();

    // Show tile stats tooltip when hovering
    if (hoveredBriefcaseIndex >= 0)
    {
        // Hovering over briefcase tile
        const auto &btiles = briefInv->getTiles();
        if (hoveredBriefcaseIndex < (int)btiles.size())
        {
            drawTileStatsTooltip(btiles[hoveredBriefcaseIndex].stat, btiles[hoveredBriefcaseIndex].type);
        }
    }
    else if (hoveredHandIndex >= 0)
    {
        // Hovering over hand tile
        const auto &ptiles = playerInventory.getTiles();
        if (hoveredHandIndex < (int)ptiles.size())
        {
            drawTileStatsTooltip(ptiles[hoveredHandIndex].stat, ptiles[hoveredHandIndex].type);
        }
    }
}
