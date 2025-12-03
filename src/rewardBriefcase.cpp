#include "rewardBriefcase.hpp"
#include "updateContext.hpp"
#include "uiManager.hpp"
#include "me.hpp"
#include <raymath.h>
#include <cmath>

RewardBriefcase::RewardBriefcase(const Vector3 &pos, const std::vector<RewardTile> &tiles)
    : position(pos), rewardTiles(tiles)
{
    // Load the briefcase model (no collision needed - it's just visual/interactive)
    this->model = LoadModel("briefcase.glb");
    if (this->model.meshCount > 0)
    {
        this->modelLoaded = true;
    }
}

RewardBriefcase::~RewardBriefcase()
{
    if (this->modelLoaded)
    {
        UnloadModel(this->model);
    }
}

void RewardBriefcase::Update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->bobTimer += delta * 2.0f;
    
    // Check for player interaction
    if (uc.player && IsPlayerNearby(uc.player->pos()))
    {
        if (!this->uiOpen && IsKeyPressed(KEY_C))
        {
            this->OpenUI();
        }
    }
}

void RewardBriefcase::Draw() const
{
    if (!this->modelLoaded)
        return;
    
    // Apply bob animation
    float bobOffset = sinf(this->bobTimer) * 0.15f;
    Vector3 drawPos = this->position;
    drawPos.y += bobOffset;
    
    // Draw the model 10x larger
    DrawModel(this->model, drawPos, 10.0f, WHITE);
    
    // Draw interaction prompt if player is nearby (handled by main loop)
}

void RewardBriefcase::DrawUI(UIManager &uiManager) const
{
    if (!this->uiOpen)
        return;
    
    // UI is drawn in the pause menu system - this is just a placeholder
    // The actual UI drawing will be handled in UIManager
}

bool RewardBriefcase::IsPlayerNearby(const Vector3 &playerPos) const
{
    float distSq = Vector3DistanceSqr(this->position, playerPos);
    float range = this->interactionRange;
    return distSq <= (range * range);
}
