#pragma once
#include <raylib.h>

class DialogBox
{
public:
    DialogBox();
    ~DialogBox();

    DialogBox(const DialogBox &) = delete;
    DialogBox &operator=(const DialogBox &) = delete;
    DialogBox(DialogBox &&) = delete;
    DialogBox &operator=(DialogBox &&) = delete;

    void setWorldPosition(const Vector3 &pos) { this->worldPosition = pos; }
    void setFillPercent(float percent);
    void setVisible(bool v) { this->visible = v; }
    bool Draw(const Camera &camera) const;

    void setBarSize(float width, float height)
    {
        this->worldBarWidth = width;
        this->worldBarHeight = height;
    }

private:
    Vector3 worldPosition{0.0f, 0.0f, 0.0f};
    float fillPercent = 1.0f;
    bool visible = true;

    float worldBarWidth = 2.5f;
    float worldBarHeight = 0.32f;
    float barVisibleDistance = 55.0f;

    Color barOutline{0, 0, 0, 220};
    Color barBackground{30, 30, 36, 220};
    Color barFill{230, 41, 55, 255};

    bool ensureTexture() const;

    static constexpr int TextureWidth = 1024;
    static constexpr int TextureHeight = 256;

    mutable RenderTexture2D barTexture{};
};
