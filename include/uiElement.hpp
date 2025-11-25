#pragma once
#include <raylib.h>

/**
 * @brief Base class for on-screen 2D UI widgets.
 *
 * Derive from UIElement and implement `draw()` and `update()`. Position and
 * size are in screen pixels. UIManager is expected to own elements added to
 * its list and handle lifetime.
 */
class UIElement
{
public:
    UIElement() {}
    virtual ~UIElement()
    {
    }

    /**
     * @brief Draw the element. Called every frame by the UI manager.
     */
    virtual void draw() = 0;

    /**
     * @brief Update element state (input handling, animations).
     */
    virtual void update() = 0;

    virtual Rectangle getBounds() const
    {
        return {position.x, position.y, size.x, size.y};
    }

protected:
    Vector2 position;
    Vector2 size;
    Texture2D *texture;
};

/**
 * @brief Simple textured quad drawn using the provided Texture2D.
 *
 * Optional `sourceRect` selects a region from a spritesheet.
 */
class UITexturedSquare : public UIElement
{
public:
    UITexturedSquare(Texture2D *texture, Vector2 position, Vector2 size);
    UITexturedSquare(Texture2D *texture, Vector2 position, Vector2 size, Rectangle sourceRect);
    ~UITexturedSquare() override = default;

    void draw() override;
    void update() override;

private:
    Rectangle sourceRect;
};

/**
 * @brief Minimal crosshair UI element.
 *
 * Draws two slim rectangles (horizontal/vertical) at the configured
 * screen position.
 */
class UICrosshair : public UIElement
{
public:
    UICrosshair(Vector2 position, int length = 10, int thickness = 2, Color color = GREEN);
    ~UICrosshair() override = default;

    void draw() override;
    void update() override;

private:
    int length;
    int thickness;
    Color color;
};