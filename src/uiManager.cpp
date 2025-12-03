#include "uiManager.hpp"
#include "attackSlotElement.hpp"
#include "rewardBriefcase.hpp"
#include <algorithm>
#include <cmath>
#include "updateContext.hpp"

const char *UIManager::slotKeyLabels[slotCount] = {"Left Click", "R", "E"};

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
    // Suppress ESC pause toggle while briefcase UI is open
    if (this->briefcaseUIOpen)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            // ignore
        }
    }
    if (pauseMenuVisible)
    {
        updatePauseMenu();
    }
    else
    {
        updateHud();
    }
}

void UIManager::draw(UpdateContext& uc)
{
    if (pauseMenuVisible)
    {
        drawPauseMenu();
    }
    else
    {
        drawHud();
    }
    if(isRewardBriefcaseUIOpen()){
        for(auto* briefcase:)
        drawBriefcaseUI(uc.scene->briefcase);
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
    
    // Update hovered tile index
    int newHoveredTileIndex = muim.getTileIndexAt(mousePos);
    if (newHoveredTileIndex >= 0 && muim.isTileUsed(newHoveredTileIndex))
    {
        newHoveredTileIndex = -1;  // Don't hover over used tiles
    }
    this->hoveredTileIndex = newHoveredTileIndex;

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
        // Check if clicking on a tile to select it
        else if (newHoveredTileIndex >= 0)
        {
            muim.selectTileByIndex(newHoveredTileIndex);
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

    // Draw player hand with selection highlight
    const auto &hand = muim.getPlayerHand();
    int selectedIdx = muim.getSelectedTileIndex();
    
    for (int i = 0; i < (int)hand.size(); ++i)
    {
        Rectangle tileBounds = muim.getTileBounds(i);
        
        // Draw selected tile higher
        if (i == selectedIdx)
        {
            tileBounds.y -= 15.0f;  // Raise selected tile
            DrawRectangleLinesEx(tileBounds, 3.0f, YELLOW);
        }
        // Draw hover indicator
        else if (i == this->hoveredTileIndex)
        {
            DrawRectangleLinesEx(tileBounds, 2.0f, Fade(WHITE, 0.7f));
        }
    }
    
    this->muim.draw();
    
    // Draw tile stats for hovered or selected tile (top-right)
    int displayIndex = this->hoveredTileIndex >= 0 ? this->hoveredTileIndex : selectedIdx;
    if (displayIndex >= 0 && displayIndex < (int)hand.size())
    {
        TileStats stats = muim.getTileStats(displayIndex);
        const float boxWidth = 200.0f;
        const float boxHeight = 100.0f;
        const float boxX = (float)GetScreenWidth() - boxWidth - 20.0f;
        const float boxY = 20.0f;
        Rectangle statsBox = {boxX, boxY, boxWidth, boxHeight};
        DrawRectangleRounded(statsBox, 0.2f, 8, Color{20, 25, 35, 230});
        DrawRectangleRoundedLines(statsBox, 0.2f, 8, Fade(RAYWHITE, 0.8f));
        const int fontSize = 16;
        DrawText("TILE STATS", (int)(boxX + 10), (int)(boxY + 10), fontSize + 2, RAYWHITE);
        char damageText[64];
        snprintf(damageText, sizeof(damageText), "Damage: %.1f", stats.damage);
        DrawText(damageText, (int)(boxX + 10), (int)(boxY + 35), fontSize, Fade(RAYWHITE, 0.9f));
        char fireRateText[64];
        snprintf(fireRateText, sizeof(fireRateText), "Fire Rate: %.2fx", stats.fireRate);
        DrawText(fireRateText, (int)(boxX + 10), (int)(boxY + 55), fontSize, Fade(RAYWHITE, 0.9f));
    }
    
    if (isDraggingTile)
    {
        drawDraggingTile();
    }
}

void UIManager::updateBriefcaseUI(RewardBriefcase *briefcase)
{
    if (!briefcase)
        return;

    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();

    // Layout constants should mirror drawBriefcaseUI
    const float briefcaseBoxX = screenW * 0.5f - 300.0f;
    const float briefcaseBoxY = 120.0f;
    const float briefcaseBoxW = 600.0f;
    const float briefcaseBoxH = 200.0f;

    auto &rewardTiles = briefcase->GetRewardTiles();

    // Compute briefcase tile layout
    const float tileBaseW = 44.0f;
    const float tileBaseH = 60.0f;
    const float caseInnerPadding = 20.0f;
    const float caseAvailW = briefcaseBoxW - caseInnerPadding * 2.0f;
    int caseCount = (int)rewardTiles.size();
    float caseGap = 12.0f;
    float caseScale = 1.0f;
    if (caseCount > 0)
    {
        float maxScaleByWidth = (caseAvailW - caseGap * (caseCount - 1)) / (caseCount * tileBaseW);
        caseScale = fminf(1.8f, fmaxf(0.6f, maxScaleByWidth));
    }
    const float tileSpacing = caseGap;
    const float tileW = tileBaseW * caseScale;
    const float tileH = tileBaseH * caseScale;
    const float rowWidth = caseCount * tileW + (caseCount - 1) * tileSpacing;
    const float tileStartX = briefcaseBoxX + (briefcaseBoxW - rowWidth) * 0.5f;
    const float tileY = briefcaseBoxY + (briefcaseBoxH - tileH) * 0.5f;

    // Compute hand layout
    const float handBoxY = briefcaseBoxY + briefcaseBoxH + 60.0f;
    const float handPadding = 24.0f;
    const float handAvailW = briefcaseBoxW - handPadding * 2.0f;
    const auto &hand = muim.getPlayerHand();
    int handCount = (int)hand.size();
    float handGap = 0.0f;
    float handScale = 1.0f;
    if (handCount > 0)
    {
        float maxScaleByWidth = (handAvailW - handGap * (handCount - 1)) / (handCount * tileBaseW);
        handScale = fminf(1.6f, fmaxf(0.5f, maxScaleByWidth));
    }
    const float handTileW = tileBaseW * handScale;
    const float handTileH = tileBaseH * handScale;
    const float handTotalWidth = handCount * handTileW + (handCount - 1) * handGap;
    const float handStartX = briefcaseBoxX + (briefcaseBoxW - handTotalWidth) * 0.5f;

    // Mouse
    Vector2 mousePos = GetMousePosition();
    bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // Update hovered indices
    hoveredBriefcaseIndex = -1;
    hoveredHandIndex = -1;
    for (int i = 0; i < caseCount; ++i)
    {
        float tileX = tileStartX + i * (tileW + tileSpacing);
        Rectangle tileRect = {tileX, tileY, tileW, tileH};
        if (CheckCollisionPointRec(mousePos, tileRect))
        {
            hoveredBriefcaseIndex = i;
            TraceLog(LOG_DEBUG, "briefcase thing hover\n");
        }
    }
    for (int i = 0; i < handCount; ++i)
    {
        float tileX = handStartX + i * (handTileW + handGap);
        Rectangle tileRect = {tileX, handBoxY, handTileW, handTileH};
        if (CheckCollisionPointRec(mousePos, tileRect))
        {
            hoveredHandIndex = i;
            TraceLog(LOG_DEBUG, "hand thing hover\n");
        }
    }

    // Handle clicks
    if (mousePressed)
    {
        // Close button
        Rectangle closeButton = {screenW * 0.5f - 70.0f, screenH - 100.0f, 140.0f, 50.0f};
        if (CheckCollisionPointRec(mousePos, closeButton))
        {
            briefcase->CloseUI();
            this->setRewardBriefcaseUIOpen(false);
            // Clear any selection state
            selectedBriefcaseIndex = -1;
            hoveredBriefcaseIndex = -1;
            hoveredHandIndex = -1;
            TraceLog(LOG_DEBUG, "close clicked\n");
            return;
        }

        // Select a briefcase tile
        if (hoveredBriefcaseIndex >= 0)
        {
            selectedBriefcaseIndex = hoveredBriefcaseIndex;
            TraceLog(LOG_DEBUG, "briefcase selected\n");
        }
        // Swap with hand tile if one is selected
        else if (hoveredHandIndex >= 0 && selectedBriefcaseIndex >= 0 && selectedBriefcaseIndex < (int)rewardTiles.size())
        {
            RewardTile picked = rewardTiles[selectedBriefcaseIndex];
            // swap types and stats
            TileType oldHandType = hand[hoveredHandIndex];
            TileStats oldStats = muim.getTileStats(hoveredHandIndex);
            muim.setTileType(hoveredHandIndex, picked.type);
            muim.setTileStats(hoveredHandIndex, picked.stats);
            // replace briefcase slot with the discarded hand tile
            rewardTiles[selectedBriefcaseIndex].type = oldHandType;
            rewardTiles[selectedBriefcaseIndex].stats = oldStats;
            // clear selection
            selectedBriefcaseIndex = -1;
        }
    }
}

void UIManager::drawBriefcaseUI(RewardBriefcase *briefcase)
{
    if (!briefcase)
        return;
    
    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    
    // Draw overlay
    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.75f));
    
    // Draw title
    const char *title = "REWARD BRIEFCASE";
    const int titleSize = 32;
    int titleWidth = MeasureText(title, titleSize);
    DrawText(title, (screenW - titleWidth) / 2, 40, titleSize, RAYWHITE);
    
    // Draw briefcase contents area
    const float briefcaseBoxX = screenW * 0.5f - 300.0f;
    const float briefcaseBoxY = 120.0f;
    const float briefcaseBoxW = 600.0f;
    const float briefcaseBoxH = 200.0f;
    
    Rectangle briefcaseBox = {briefcaseBoxX, briefcaseBoxY, briefcaseBoxW, briefcaseBoxH};
    DrawRectangleRounded(briefcaseBox, 0.15f, 8, Color{30, 35, 45, 240});
    DrawRectangleRoundedLines(briefcaseBox, 0.15f, 8, Fade(RAYWHITE, 0.8f));
    
    const char *briefcaseLabel = "Briefcase Contents (Click to take)";
    DrawText(briefcaseLabel, (int)briefcaseBoxX + 10, (int)briefcaseBoxY - 25, 18, Fade(RAYWHITE, 0.8f));
    
    // Draw reward tiles with hover stats and selection
    auto &rewardTiles = briefcase->GetRewardTiles();
    const float tileBaseW = 44.0f;
    const float tileBaseH = 60.0f;
    const float caseInnerPadding = 20.0f;
    const float caseAvailW = briefcaseBoxW - caseInnerPadding * 2.0f;
    int caseCount = (int)rewardTiles.size();
    float caseGap = 12.0f;
    float caseScale = 1.0f;
    if (caseCount > 0)
    {
        float maxScaleByWidth = (caseAvailW - caseGap * (caseCount - 1)) / (caseCount * tileBaseW);
        caseScale = fminf(1.8f, fmaxf(0.6f, maxScaleByWidth));
    }
    const float tileSpacing = caseGap;
    const float tileW = tileBaseW * caseScale;
    const float tileH = tileBaseH * caseScale;
    const float rowWidth = caseCount * tileW + (caseCount - 1) * tileSpacing;
    const float tileStartX = briefcaseBoxX + (briefcaseBoxW - rowWidth) * 0.5f;
    const float tileY = briefcaseBoxY + (briefcaseBoxH - tileH) * 0.5f;
    
    Vector2 mousePos = GetMousePosition();
    
    for (int i = 0; i < (int)rewardTiles.size(); ++i)
    {
        float tileX = tileStartX + i * (tileW + tileSpacing);
        Rectangle tileRect = {tileX, tileY, tileW, tileH};
        
        bool hovered = (hoveredBriefcaseIndex == i) || CheckCollisionPointRec(mousePos, tileRect);
        
        // Draw tile background
        DrawRectangleRounded(tileRect, 0.15f, 6, hovered ? Fade(YELLOW, 0.3f) : Fade(WHITE, 0.1f));
        Color border = (this->selectedBriefcaseIndex == i) ? ORANGE : (hovered ? YELLOW : Fade(RAYWHITE, 0.4f));
        DrawRectangleRoundedLines(tileRect, 0.15f, 6, border);
        
        // Draw tile texture
        Rectangle source = muim.getTile(rewardTiles[i].type);
        Rectangle dest = {tileX + tileW * 0.05f, tileY + tileH * 0.05f, tileW * 0.90f, tileH * 0.90f};
        DrawTexturePro(muim.getSpriteSheet(), source, dest, {0, 0}, 0, WHITE);
        
        // Draw stats below tile
        char statsText[64];
        snprintf(statsText, sizeof(statsText), "DMG:%.0f FR:%.1fx", rewardTiles[i].stats.damage, rewardTiles[i].stats.fireRate);
        DrawText(statsText, (int)tileX, (int)(tileY + tileRect.height + 6), 14, Fade(RAYWHITE, 0.7f));
        
    }

    // Draw player hand area with hover and swap target
    const float handBoxY = briefcaseBoxY + briefcaseBoxH + 60.0f;
    const char *handLabel = "Your Hand (Click a tile to swap)";
    DrawText(handLabel, (int)briefcaseBoxX + 10, (int)handBoxY - 25, 18, Fade(RAYWHITE, 0.8f));
    
    const auto &hand = muim.getPlayerHand();
    // Fit the entire hand on-screen with consistent look
    const float handPadding = 24.0f;
    const float handAvailW = briefcaseBoxW - handPadding * 2.0f;
    int handCount = (int)hand.size();
    float handGap = 12.0f;
    float handScale = 1.0f;
    if (handCount > 0)
    {
        float maxScaleByWidth = (handAvailW - handGap * (handCount - 1)) / (handCount * tileBaseW);
        handScale = fminf(1.6f, fmaxf(0.5f, maxScaleByWidth));
    }
    const float handTileW = tileBaseW * handScale;
    const float handTileH = tileBaseH * handScale;
    const float handTotalWidth = handCount * handTileW + (handCount - 1) * handGap;
    const float handStartX = briefcaseBoxX + (briefcaseBoxW - handTotalWidth) * 0.5f;
    for (int i = 0; i < (int)hand.size(); ++i)
    {
        float tileX = handStartX + i * (handTileW + handGap);
        Rectangle tileRect = {tileX, handBoxY, handTileW, handTileH};
        bool hovered = (hoveredHandIndex == i) || CheckCollisionPointRec(mousePos, tileRect);
        DrawRectangleRounded(tileRect, 0.15f, 6, hovered ? Fade(SKYBLUE, 0.25f) : Fade(WHITE, 0.08f));
        DrawRectangleRoundedLines(tileRect, 0.15f, 6, hovered ? SKYBLUE : Fade(RAYWHITE, 0.4f));
        Rectangle src = muim.getTile(hand[i]);
        Rectangle dest = {tileX + handTileW * 0.05f, handBoxY + handTileH * 0.05f, handTileW * 0.90f, handTileH * 0.90f};
        DrawTexturePro(muim.getSpriteSheet(), src, dest, {0,0}, 0.0f, WHITE);
        // Hover stats (top-right overlay)
        if (hovered)
        {
            TileStats stats = muim.getTileStats(i);
            const float boxW = 180.0f;
            const float boxH = 80.0f;
            float boxX = (float)GetScreenWidth() - boxW - 20.0f;
            float boxY = 20.0f;
            Rectangle statsBox = {boxX, boxY, boxW, boxH};
            DrawRectangleRounded(statsBox, 0.2f, 8, Color{20,25,35,230});
            DrawRectangleRoundedLines(statsBox, 0.2f, 8, Fade(RAYWHITE, 0.8f));
            DrawText("TILE STATS", (int)(boxX + 10), (int)(boxY + 8), 18, RAYWHITE);
            char t1[64]; snprintf(t1, sizeof(t1), "Damage: %.1f", stats.damage);
            DrawText(t1, (int)(boxX + 10), (int)(boxY + 30), 16, RAYWHITE);
            char t2[64]; snprintf(t2, sizeof(t2), "Fire Rate: %.2fx", stats.fireRate);
            DrawText(t2, (int)(boxX + 10), (int)(boxY + 48), 16, RAYWHITE);
        }
    }
    
    // Reminder: player hand drawn above
    
    // Draw close button
    Rectangle closeButton = {screenW * 0.5f - 70.0f, screenH - 100.0f, 140.0f, 50.0f};
    bool closeHovered = CheckCollisionPointRec(mousePos, closeButton);
    DrawRectangleRounded(closeButton, 0.2f, 8, closeHovered ? Color{80, 130, 200, 220} : Color{50, 80, 120, 220});
    DrawRectangleRoundedLines(closeButton, 0.2f, 8, Fade(RAYWHITE, 0.9f));
    
    const char *closeText = "Close";
    int closeTextWidth = MeasureText(closeText, 24);
    DrawText(closeText, (int)(closeButton.x + closeButton.width / 2 - closeTextWidth / 2), 
             (int)(closeButton.y + 13), 24, RAYWHITE);
    
    // Close click handled in updateBriefcaseUI
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
    if (draggingTile.tile == TileType::EMPTY)
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
    if (!isValidSlotIndex(slotIndex) || entry.tile == TileType::EMPTY || entry.handIndex < 0)
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