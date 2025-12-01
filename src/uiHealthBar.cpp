#include "uiHealthBar.hpp"
#include "me.hpp"

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
