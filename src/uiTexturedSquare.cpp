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

// --- UICrosshair Implementation ---

UICrosshair::UICrosshair(Vector2 _position, int _length, int _thickness, Color _color)
{
    this->position = _position;
    this->length = _length;
    this->thickness = _thickness;
    this->color = _color;
}

void UICrosshair::update() 
{
    // Static crosshair, no update logic needed
}

void UICrosshair::draw()
{
    // Draw horizontal line
    DrawRectangle(position.x - length, position.y - thickness / 2.0f, length * 2, thickness, color);
    // Draw vertical line
    DrawRectangle(position.x - thickness / 2.0f, position.y - length, thickness, length * 2, color);
}