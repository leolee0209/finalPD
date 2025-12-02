#include "uiManager.hpp"
#include "raylib.h"
#include <iostream>

void MahjongUIManager::addTile(MahjongTileType type, Vector2 position)
{
    Rectangle sourceRect = getTile(type);
    Vector2 size = {(float)sourceRect.width, (float)sourceRect.height};
    elements.push_back(new UITexturedSquare(&this->spriteSheet, position, size, sourceRect));
    tileHitboxes.push_back({position.x, position.y, size.x, size.y});
}

void MahjongUIManager::createPlayerHand(int screenWidth, int screenHeight)
{
    this->elements.clear();
    this->playerHand.clear();
    this->tileHitboxes.clear();
    this->tileUsed.clear();

    const float startY = screenHeight - this->tileHeight - 10.0f; // 10px margin from bottom

    playerHand.push_back(MahjongTileType::CHARACTER_1);
    playerHand.push_back(MahjongTileType::CHARACTER_2);
    playerHand.push_back(MahjongTileType::CHARACTER_3);
    playerHand.push_back(MahjongTileType::BAMBOO_6);
    playerHand.push_back(MahjongTileType::BAMBOO_6);
    playerHand.push_back(MahjongTileType::BAMBOO_6);
    playerHand.push_back(MahjongTileType::DOT_9);
    playerHand.push_back(MahjongTileType::DOT_9);
    playerHand.push_back(MahjongTileType::DOT_9);
    playerHand.push_back(MahjongTileType::WIND_EAST);
    playerHand.push_back(MahjongTileType::WIND_WEST);
    playerHand.push_back(MahjongTileType::DRAGON_RED);
    playerHand.push_back(MahjongTileType::DRAGON_GREEN);
    playerHand.push_back(MahjongTileType::CHARACTER_9);
    playerHand.push_back(MahjongTileType::CHARACTER_9);
    playerHand.push_back(MahjongTileType::CHARACTER_9);
    playerHand.push_back(MahjongTileType::BAMBOO_1);

    const int handSize = playerHand.size();
    const float totalHandWidth = handSize * this->tileWidth;
    const float startX = (screenWidth - totalHandWidth) / 2.0f;

    for (int i = 0; i < handSize; i++)
    {
        addTile(playerHand[i], {startX + i * this->tileWidth, startY});
    }

    tileUsed.assign(handSize, false);
}

void MahjongUIManager::update()
{
    float mouseWheelMove = GetMouseWheelMove();
    if (mouseWheelMove > 0)
    {
        nextTile();
    }
    if (mouseWheelMove < 0)
    {
        previousTile();
    }

    for (auto const &element : elements)
    {
        element->update();
    }
}

void MahjongUIManager::draw()
{
    for (int i = 0; i < elements.size(); i++)
    {
        elements[i]->draw();
        if (i == selectedTileIndex)
        {
            // Draw a rectangle around the selected tile
            DrawRectangleLinesEx(elements[i]->getBounds(), 2.0f, YELLOW);
        }
        if (i < tileUsed.size() && tileUsed[i])
        {
            Rectangle bounds = elements[i]->getBounds();
            DrawRectangleRec(bounds, Fade(DARKGRAY, 0.5f));
        }
    }
}

void MahjongUIManager::nextTile()
{
    selectedTileIndex = (selectedTileIndex + 1) % playerHand.size();
}

void MahjongUIManager::previousTile()
{
    selectedTileIndex = (selectedTileIndex - 1 + playerHand.size()) % playerHand.size();
}

MahjongTileType MahjongUIManager::getSelectedTile()
{
    if (selectedTileIndex >= 0 && selectedTileIndex < playerHand.size())
    {
        return playerHand[selectedTileIndex];
    }
    return MahjongTileType::EMPTY;
}

void MahjongUIManager::selectTileByType(MahjongTileType type)
{
    for (int i = 0; i < playerHand.size(); ++i)
    {
        if (playerHand[i] == type)
        {
            selectedTileIndex = i;
            return;
        }
    }
    if (!playerHand.empty())
    {
        selectedTileIndex = 0;
    }
}

void MahjongUIManager::selectTileByIndex(int index)
{
    if (index >= 0 && index < playerHand.size())
    {
        selectedTileIndex = index;
    }
}

int MahjongUIManager::getTileIndexAt(Vector2 position) const
{
    for (int i = 0; i < tileHitboxes.size(); ++i)
    {
        if (CheckCollisionPointRec(position, tileHitboxes[i]))
        {
            return i;
        }
    }
    return -1;
}

Rectangle MahjongUIManager::getTileBounds(int index) const
{
    if (index < 0 || index >= tileHitboxes.size())
    {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }
    return tileHitboxes[index];
}

MahjongTileType MahjongUIManager::getTileTypeAt(int index) const
{
    if (index < 0 || index >= playerHand.size())
        return MahjongTileType::EMPTY;
    return playerHand[index];
}

bool MahjongUIManager::isTileUsed(int index) const
{
    if (index < 0 || index >= tileUsed.size())
        return false;
    return tileUsed[index];
}

void MahjongUIManager::setTileUsed(int index, bool used)
{
    if (index < 0)
        return;
    if (index >= tileUsed.size())
        tileUsed.resize(index + 1, false);
    tileUsed[index] = used;
}
