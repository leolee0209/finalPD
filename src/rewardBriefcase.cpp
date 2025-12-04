#include "rewardBriefcase.hpp"
#include "updateContext.hpp"
#include "uiManager.hpp"
#include "me.hpp"
#include <raymath.h>
#include <cmath>

// Static members
Model RewardBriefcase::sharedModel = {0};
bool RewardBriefcase::modelLoaded = false;

void RewardBriefcase::LoadSharedModel()
{
    if (!modelLoaded)
    {
        sharedModel = LoadModel("briefcase.glb");
        if (sharedModel.meshCount > 0)
        {
            modelLoaded = true;
        }
    }
}

void RewardBriefcase::UnloadSharedModel()
{
    if (modelLoaded && IsWindowReady())
    {
        UnloadModel(sharedModel);
        modelLoaded = false;
    }
}

RewardBriefcase::RewardBriefcase(const Vector3 &pos, Inventory inv)
    : position(pos), inventory(std::move(inv))
{
}

RewardBriefcase::~RewardBriefcase()
{
}

void RewardBriefcase::Update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->bobTimer += delta * 2.0f;
}

void RewardBriefcase::Draw() const
{
    if (!modelLoaded)
        return;

    float bobOffset = sinf(this->bobTimer) * 0.15f;
    Vector3 drawPos = this->position;
    drawPos.y += bobOffset;
    DrawModel(sharedModel, drawPos, 10.0f, WHITE);
}

// Briefcase has no UI ownership; UIManager renders menu when activated

bool RewardBriefcase::IsPlayerNearby(const Vector3 &playerPos) const
{
    float distSq = Vector3DistanceSqr(this->position, playerPos);
    float range = this->interactionRange;
    return distSq <= (range * range);
}
