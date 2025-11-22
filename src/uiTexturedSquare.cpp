#include "uiElement.hpp"

UITexturedSquare::UITexturedSquare(Texture2D *_texture, Vector2 _position, Vector2 _size)
{
    this->texture = _texture;
    this->position = _position;
    this->size = _size;
    this->sourceRect = {0.0f, 0.0f, (float)texture->width, (float)texture->height};
}

UITexturedSquare::UITexturedSquare(Texture2D *_texture, Vector2 _position, Vector2 _size, Rectangle _sourceRect)
{
    this->texture = _texture;
    this->position = _position;
    this->size = _size;
    this->sourceRect = _sourceRect;
}

void UITexturedSquare::draw()
{
    DrawTextureRec(*texture, sourceRect, position, WHITE);
}

void UITexturedSquare::update()
{
    // Nothing to update for a static textured square
}
