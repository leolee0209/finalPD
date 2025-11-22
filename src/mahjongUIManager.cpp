#include "uiManager.hpp"
#include "raylib.h"
#include <iostream>

void MahjongUIManager::addTile(MahjongTileType type, Vector2 position)
{
    Rectangle sourceRect = getTile(type);
    Vector2 size = {(float)sourceRect.width, (float)sourceRect.height};
    elements.push_back(new UITexturedSquare(&this->spriteSheet, position, size, sourceRect));
}

void MahjongUIManager::createPlayerHand(int screenWidth, int screenHeight)
{
    this->elements.clear();
    this->playerHand.clear();

    const float startY = screenHeight - this->tileHeight - 10.0f; // 10px margin from bottom

    playerHand.push_back(MahjongTileType::CHARACTER_1);
    playerHand.push_back(MahjongTileType::CHARACTER_2);
    playerHand.push_back(MahjongTileType::CHARACTER_3);
    playerHand.push_back(MahjongTileType::BAMBOO_4);
    playerHand.push_back(MahjongTileType::BAMBOO_5);
    playerHand.push_back(MahjongTileType::BAMBOO_6);
    playerHand.push_back(MahjongTileType::DOT_7);
    playerHand.push_back(MahjongTileType::DOT_8);
    playerHand.push_back(MahjongTileType::DOT_9);
    playerHand.push_back(MahjongTileType::WIND_EAST);
    playerHand.push_back(MahjongTileType::WIND_WEST);
    playerHand.push_back(MahjongTileType::DRAGON_RED);
    playerHand.push_back(MahjongTileType::DRAGON_GREEN);
    playerHand.push_back(MahjongTileType::CHARACTER_9);
    playerHand.push_back(MahjongTileType::CHARACTER_9);
    playerHand.push_back(MahjongTileType::BAMBOO_1);
    playerHand.push_back(MahjongTileType::BAMBOO_1);

    const int handSize = playerHand.size();
    const float totalHandWidth = handSize * this->tileWidth;
    const float startX = (screenWidth - totalHandWidth) / 2.0f;

    for (int i = 0; i < handSize; i++)
    {
        addTile(playerHand[i], {startX + i * this->tileWidth, startY});
    }
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
