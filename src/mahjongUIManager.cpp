#include "uiManager.hpp"
#include "raylib.h"
#include <iostream>
#include "Inventory.hpp"

void MahjongUIManager::createHandUI(Inventory &inventory, int screenWidth, int screenHeight)
{
    auto &tiles = inventory.getTiles();
    
    // Clear existing UI elements if size changed
    if (handElements.size() != tiles.size())
    {
        for (auto *elem : handElements)
        {
            delete elem;
        }
        handElements.clear();
        tileHitboxes.clear();
        tileUsed.clear();
        
        // Create new UI elements for each tile
        const int handSize = tiles.size();
        const float totalHandWidth = handSize * this->tileWidth;
        const float startX = (screenWidth - totalHandWidth) / 2.0f;
        const float startY = screenHeight - this->tileHeight - 10.0f;
        
        for (int i = 0; i < handSize; ++i)
        {
            float x = startX + i * this->tileWidth;
            Rectangle source = getTile(tiles[i].type);
            UITexturedSquare *elem = new UITexturedSquare(&spriteSheet, {x, startY}, 
                                                          {(float)this->tileWidth, (float)this->tileHeight}, 
                                                          source);
            handElements.push_back(elem);
            tileHitboxes.push_back(elem->getBounds());
        }
        
        tileUsed.resize(handSize, false);
    }
    else
    {
        // Update existing elements with new tile types - need to recreate them
        // For simplicity, just recreate if types changed
        tiles = inventory.getTiles();
        for (int i = 0; i < tiles.size(); ++i)
        {
            Rectangle source = getTile(tiles[i].type);
            if (UITexturedSquare *elem = dynamic_cast<UITexturedSquare*>(handElements[i]))
            {
                // Recreate element with new source rect
                Vector2 pos = {elem->getBounds().x, elem->getBounds().y};
                Vector2 sz = {elem->getBounds().width, elem->getBounds().height};
                delete handElements[i];
                handElements[i] = new UITexturedSquare(&spriteSheet, pos, sz, source);
                tileHitboxes[i] = handElements[i]->getBounds();
            }
        }
    }
}

void MahjongUIManager::update(Inventory &inventory)
{
    // Tile selection via mouse wheel is disabled - only allow selection in pause menu by clicking
    
    for (auto const &element : handElements)
    {
        element->update();
    }
}

void MahjongUIManager::draw()
{
    for (int i = 0; i < handElements.size(); i++)
    {
        // Apply offset for selected tile to make it "pop up"
        float yOffset = 0.0f;
        if (i == selectedTileIndex)
        {
            yOffset = -15.0f; // Raise the selected tile
        }
        
        // Get original bounds
        Rectangle originalBounds = handElements[i]->getBounds();
        
        // Check if it's a UITexturedSquare to access tile texture
        if (UITexturedSquare *elem = dynamic_cast<UITexturedSquare*>(handElements[i]))
        {
            Vector2 pos = {originalBounds.x, originalBounds.y + yOffset};
            Vector2 sz = {originalBounds.width, originalBounds.height};
            
            // Draw the tile texture with offset
            Rectangle source = elem->getSourceRect();
            DrawTexturePro(spriteSheet, source, 
                          {pos.x, pos.y, sz.x, sz.y}, 
                          {0, 0}, 0.0f, WHITE);
            
            // Draw selection indicator
            if (i == selectedTileIndex)
            {
                DrawRectangleLinesEx({pos.x, pos.y, sz.x, sz.y}, 3.0f, YELLOW);
            }
            
            // Draw used overlay
            if (i < tileUsed.size() && tileUsed[i])
            {
                DrawRectangleRec({pos.x, pos.y, sz.x, sz.y}, Fade(DARKGRAY, 0.5f));
            }
        }
        else
        {
            // Fallback for non-UITexturedSquare elements - shouldn't happen
            handElements[i]->draw();
            if (i == selectedTileIndex)
            {
                DrawRectangleLinesEx(originalBounds, 2.0f, YELLOW);
            }
            if (i < tileUsed.size() && tileUsed[i])
            {
                DrawRectangleRec(originalBounds, Fade(DARKGRAY, 0.5f));
            }
        }
    }
}

void MahjongUIManager::nextTile(Inventory &inventory)
{
    auto &tiles = inventory.getTiles();
    if (!tiles.empty())
    {
        selectedTileIndex = (selectedTileIndex + 1) % tiles.size();
    }
}

void MahjongUIManager::previousTile(Inventory &inventory)
{
    auto &tiles = inventory.getTiles();
    if (!tiles.empty())
    {
        selectedTileIndex = (selectedTileIndex - 1 + tiles.size()) % tiles.size();
    }
}

TileType MahjongUIManager::getSelectedTile(Inventory &inventory)
{
    auto &tiles = inventory.getTiles();
    if (selectedTileIndex >= 0 && selectedTileIndex < tiles.size())
    {
        return tiles[selectedTileIndex].type;
    }
    return TileType::EMPTY;
}

void MahjongUIManager::selectTileByType(Inventory &inventory, TileType type)
{
    auto &tiles = inventory.getTiles();
    for (int i = 0; i < tiles.size(); ++i)
    {
        if (tiles[i].type == type)
        {
            selectedTileIndex = i;
            return;
        }
    }
    if (!tiles.empty())
    {
        selectedTileIndex = 0;
    }
}

void MahjongUIManager::selectTileByIndex(int index)
{
    selectedTileIndex = index;
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
