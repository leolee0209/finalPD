#pragma once
#include <raylib.h>

class UIElement
{
public:
    UIElement() {}
    virtual ~UIElement()
    {
    }

    virtual void draw() = 0;
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

// New Class: Simple crosshair drawn with rectangles
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