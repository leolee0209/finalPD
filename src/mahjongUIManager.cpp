#include "uiManager.hpp"



void MahjongUIManager::addTile(MahjongTileType type, Vector2 position)
{
    Rectangle sourceRect = getTile(type);
    Vector2 size = {(float)sourceRect.width, (float)sourceRect.height};
    elements.push_back(new UITexturedSquare(&this->spriteSheet, position, size, sourceRect));
}

void MahjongUIManager::update()
{
    for (auto const &element : elements)
    {
        element->update();
    }
}

void MahjongUIManager::draw()
{
    for (auto const &element : elements)
    {
        element->draw();
    }
}
