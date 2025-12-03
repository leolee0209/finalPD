#include "rewardBriefcase.hpp"
#include "updateContext.hpp"
#include "uiManager.hpp"
#include "me.hpp"
#include <raymath.h>
#include <cmath>

RewardBriefcase::RewardBriefcase(const Vector3 &pos, Inventory inv)
    : position(pos), inventory(std::move(inv))
{
    this->model = LoadModel("briefcase.glb");
    if (this->model.meshCount > 0)
    {
        this->modelLoaded = true;
    }
}

RewardBriefcase::~RewardBriefcase()
{
    if (this->modelLoaded && IsWindowReady())
    {
        UnloadModel(this->model);
    }
}

void RewardBriefcase::Update(UpdateContext &uc)
{
    float delta = GetFrameTime();
    this->bobTimer += delta * 2.0f;
}

void RewardBriefcase::Draw() const
{
    if (!this->modelLoaded)
        return;

    float bobOffset = sinf(this->bobTimer) * 0.15f;
    Vector3 drawPos = this->position;
    drawPos.y += bobOffset;
    DrawModel(this->model, drawPos, 10.0f, WHITE);
}

// Briefcase has no UI ownership; UIManager renders menu when activated

bool RewardBriefcase::IsPlayerNearby(const Vector3 &playerPos) const
{
    float distSq = Vector3DistanceSqr(this->position, playerPos);
    float range = this->interactionRange;
    return distSq <= (range * range);
}
