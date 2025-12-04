#pragma once

#include <raylib.h>
#include <vector>
#include <memory>
#include "Inventory.hpp"
#include "me.hpp" // for Entity base class

class UpdateContext;
class UIManager;

class RewardBriefcase : public Entity
{
public:
    RewardBriefcase(const Vector3 &position, Inventory inventory);
    ~RewardBriefcase();
    
    void Update(UpdateContext &uc);
    void Draw() const;
    
    bool IsPlayerNearby(const Vector3 &playerPos) const;
    bool IsActivated() const { return this->activated; }
    void SetActivated(bool active) { this->activated = active; }
    
    Inventory &GetInventory() { return this->inventory; }
    const Inventory &GetInventory() const { return this->inventory; }
    
    Vector3 GetPosition() const { return this->position; }

    // Entity overrides
    void UpdateBody(UpdateContext &uc) override { Update(uc); }
    EntityCategory category() const override { return ENTITY_ALL; }
    
    // Static model shared by all briefcases
    static Model sharedModel;
    static bool modelLoaded;
    static void LoadSharedModel();
    static void UnloadSharedModel();
    
private:
    Vector3 position;
    Inventory inventory;
    bool activated = false;
    float interactionRange = 3.0f;
    float bobTimer = 0.0f;
};
