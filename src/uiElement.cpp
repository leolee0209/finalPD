#include "uiElement.hpp"
#include "me.hpp"
#include "uiManager.hpp"
#include "Inventory.hpp"

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

UIHealthBar::UIHealthBar(Me *playerRef, int maxHealthValue, float width, float height)
{
    this->player = playerRef;
    this->maxHealth = maxHealthValue > 0 ? maxHealthValue : 1;
    this->size = {width, height};
    this->position = {margin, 0.0f};
}

void UIHealthBar::setPlayer(Me *playerRef)
{
    this->player = playerRef;
}

void UIHealthBar::setMaxHealth(int maxHealthValue)
{
    this->maxHealth = maxHealthValue > 0 ? maxHealthValue : 1;
}

void UIHealthBar::setMargin(float marginPixels)
{
    this->margin = marginPixels;
}

void UIHealthBar::setOutlineThickness(float thicknessPixels)
{
    this->outlineThickness = thicknessPixels < 0.0f ? 0.0f : thicknessPixels;
}

void UIHealthBar::setColors(Color base, Color fill, Color outline)
{
    this->baseColor = base;
    this->fillColor = fill;
    this->outlineColor = outline;
}

void UIHealthBar::update()
{
    float percent = 0.0f;
    if (this->player && this->maxHealth > 0)
    {
        percent = static_cast<float>(this->player->getHealth()) / static_cast<float>(this->maxHealth);
    }
    if (percent < 0.0f)
    {
        percent = 0.0f;
    }
    if (percent > 1.0f)
    {
        percent = 1.0f;
    }
    this->displayedPercent = percent;
}

void UIHealthBar::draw()
{
    float screenHeight = static_cast<float>(GetScreenHeight());
    Rectangle baseRect = {this->margin, screenHeight - this->margin - this->size.y, this->size.x, this->size.y};

    DrawRectangleRec(baseRect, this->baseColor);

    Rectangle fillRect = baseRect;
    fillRect.width = baseRect.width * this->displayedPercent;
    DrawRectangleRec(fillRect, this->fillColor);

    if (this->outlineThickness > 0.0f)
    {
        DrawRectangleLinesEx(baseRect, this->outlineThickness, this->outlineColor);
    }
}

// --- UISelectedTileDisplay Implementation ---

UISelectedTileDisplay::UISelectedTileDisplay(MahjongUIManager *muimPtr, Inventory *inv)
    : muim(muimPtr), inventory(inv)
{
    this->position = {0, 0};
    this->size = {60, 80}; // Tile size
}

void UISelectedTileDisplay::update()
{
    // Nothing to update
}

void UISelectedTileDisplay::draw()
{
    if (!muim || !inventory)
        return;

    int selectedIdx = muim->getSelectedTileIndex();
    auto &tiles = inventory->getTiles();
    
    if (selectedIdx < 0 || selectedIdx >= (int)tiles.size())
        return;

    TileType selectedType = tiles[selectedIdx].type;
    
    // Position above health bar, slightly to the right
    float screenHeight = static_cast<float>(GetScreenHeight());
    float margin = 20.0f;
    float healthBarHeight = 20.0f;
    
    // Calculate position: above health bar with some spacing
    Vector2 pos = {margin + 10.0f, screenHeight - margin - healthBarHeight - this->size.y - 10.0f};
    
    // Draw background frame
    Rectangle frame = {pos.x - 5.0f, pos.y - 5.0f, this->size.x + 10.0f, this->size.y + 10.0f};
    DrawRectangleRounded(frame, 0.2f, 4, Color{20, 25, 35, 200});
    DrawRectangleRoundedLines(frame, 0.2f, 4, Fade(YELLOW, 0.8f));
    
    // Draw the tile
    Rectangle source = muim->getTile(selectedType);
    Rectangle dest = {pos.x, pos.y, this->size.x, this->size.y};
    DrawTexturePro(muim->getSpriteSheet(), source, dest, {0, 0}, 0.0f, WHITE);
    
    // // Draw "BASIC ATTACK" label below
    // const char *label = "BASIC";
    // int fontSize = 12;
    // int textWidth = MeasureText(label, fontSize);
    // DrawText(label, 
    //          (int)(pos.x + this->size.x / 2.0f - textWidth / 2.0f),
    //          (int)(pos.y + this->size.y + 2.0f),
    //          fontSize,
    //          YELLOW);
}
