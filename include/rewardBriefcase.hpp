#pragma once

#include <raylib.h>
#include <vector>
#include <memory>
#include "Inventory.hpp"

class UpdateContext;
class UIManager;

class RewardBriefcase
{
public:
    RewardBriefcase(const Vector3 &position, Inventory &inventory);
    ~RewardBriefcase();
    
    void Update(UpdateContext &uc);
    void Draw() const;
    void DrawUI(UIManager &uiManager) const;
    
    bool IsPlayerNearby(const Vector3 &playerPos) const;
    bool IsUIOpen() const { return this->uiOpen; }
    void OpenUI() { this->uiOpen = true; }
    void CloseUI() { this->uiOpen = false; }
    
    Inventory &GetInventory() { return this->inventory; }
    const Inventory &GetInventory() const { return this->inventory; }
    
    Vector3 GetPosition() const { return this->position; }
    
private:
    Vector3 position;
    Inventory &inventory;
    Model model;
    bool modelLoaded = false;
    bool uiOpen = false;
    float interactionRange = 3.0f;
    float bobTimer = 0.0f;
};
