#include "dialogBox.hpp"
#include <raymath.h>
#include <cmath>

DialogBox::DialogBox() = default;

DialogBox::~DialogBox()
{
    // Guard: Unload only while window/context is alive to avoid segfaults
    if (this->barTexture.id != 0)
    {
        if (IsWindowReady())
        {
            UnloadRenderTexture(this->barTexture);
        }
        this->barTexture = {};
    }
}

bool DialogBox::ensureTexture() const
{
    if (this->barTexture.id != 0)
        return true;

    RenderTexture2D tex = LoadRenderTexture(TextureWidth, TextureHeight);
    if (tex.id == 0)
        return false;

    SetTextureFilter(tex.texture, TEXTURE_FILTER_BILINEAR);
    this->barTexture = tex;
    return true;
}

void DialogBox::setFillPercent(float percent)
{
    this->fillPercent = Clamp(percent, 0.0f, 1.0f);
}

bool DialogBox::Draw(const Camera &camera) const
{
    if (!this->visible)
        return false;

    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    if (Vector3LengthSqr(camForward) < 0.0001f)
        camForward = {0.0f, 0.0f, -1.0f};
    Vector3 camRight = Vector3CrossProduct(camForward, {0.0f, 1.0f, 0.0f});
    if (Vector3LengthSqr(camRight) < 0.0001f)
        camRight = {1.0f, 0.0f, 0.0f};
    else
        camRight = Vector3Normalize(camRight);
    Vector3 camUp = Vector3CrossProduct(camRight, camForward);
    if (Vector3LengthSqr(camUp) < 0.0001f)
        camUp = {0.0f, 1.0f, 0.0f};
    else
        camUp = Vector3Normalize(camUp);

    Vector3 toDialog = Vector3Subtract(this->worldPosition, camera.position);
    float distSq = Vector3LengthSqr(toDialog);
    if (distSq < 0.0001f)
        return false;
    float dist = sqrtf(distSq);
    if (dist > this->barVisibleDistance)
        return false;
    Vector3 toDialogDir = Vector3Scale(toDialog, 1.0f / dist);
    if (Vector3DotProduct(camForward, toDialogDir) <= 0.0f)
        return false;

    Vector2 screenPos = GetWorldToScreen(this->worldPosition, camera);
    if (screenPos.x < 0.0f || screenPos.x > (float)GetScreenWidth() ||
        screenPos.y < 0.0f || screenPos.y > (float)GetScreenHeight())
    {
        return false;
    }

    Vector3 barCenter = this->worldPosition;
    Vector3 halfRight = Vector3Scale(camRight, this->worldBarWidth * 0.5f);
    Vector3 halfUp = Vector3Scale(camUp, this->worldBarHeight * 0.5f);

    Vector2 screenLeft = GetWorldToScreen(Vector3Subtract(barCenter, halfRight), camera);
    Vector2 screenRight = GetWorldToScreen(Vector3Add(barCenter, halfRight), camera);
    Vector2 screenTop = GetWorldToScreen(Vector3Add(barCenter, halfUp), camera);
    Vector2 screenBottom = GetWorldToScreen(Vector3Subtract(barCenter, halfUp), camera);

    float pixelWidth = Vector2Distance(screenLeft, screenRight);
    float pixelHeight = fabsf(screenTop.y - screenBottom.y);
    if (pixelWidth < 4.0f || pixelHeight < 2.0f)
        return false;

    Rectangle barBack{screenPos.x - pixelWidth * 0.5f,
                      screenPos.y - pixelHeight,
                      pixelWidth,
                      pixelHeight};

    if (!this->ensureTexture())
        return false;

    BeginTextureMode(this->barTexture);
    ClearBackground(BLANK);

    float padding = TextureHeight * 0.15f;
    Rectangle texBar{padding,
                     padding,
                     (float)TextureWidth - padding * 2.0f,
                     (float)TextureHeight - padding * 2.0f};

    DrawRectangleRounded(texBar, 0.45f, 6, this->barBackground);
    DrawRectangleRoundedLines(texBar, 0.45f, 6, this->barOutline);

    if (this->fillPercent > 0.0f)
    {
        Rectangle fillRect = texBar;
        fillRect.width = texBar.width * this->fillPercent;
        DrawRectangleRounded(fillRect, 0.4f, 6, this->barFill);
    }

    EndTextureMode();

    Rectangle srcRect{0.0f, 0.0f, (float)TextureWidth, -(float)TextureHeight};
    Rectangle destRect{barBack.x, barBack.y, barBack.width, barBack.height};
    DrawTexturePro(this->barTexture.texture, srcRect, destRect, {0.0f, 0.0f}, 0.0f, WHITE);

    return true;
}
