#pragma once

#include "uiElement.hpp"
#include "constant.hpp"

class Me;

/**
 * @brief HUD element that visualizes the player's health.
 */
class UIHealthBar : public UIElement
{
public:
    UIHealthBar(Me *player, int maxHealth = MAX_HEALTH_ME, float width = 280.0f, float height = 20.0f);
    ~UIHealthBar() override = default;

    void setPlayer(Me *player);
    void setMaxHealth(int maxHealth);
    void setMargin(float marginPixels);
    void setOutlineThickness(float thicknessPixels);
    void setColors(Color base, Color fill, Color outline);

    void update() override;
    void draw() override;

private:
    Me *player = nullptr;
    int maxHealth = 1;
    float margin = 20.0f;
    float outlineThickness = 2.0f;
    Color baseColor{60, 60, 60, 255};
    Color fillColor{230, 41, 55, 255};
    Color outlineColor{0, 0, 0, 255};
    float displayedPercent = 1.0f;
};
