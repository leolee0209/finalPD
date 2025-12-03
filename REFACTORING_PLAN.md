# UI Refactoring Plan - Inventory Integration

## Goal
Refactor UI classes to work with the new Inventory class instead of managing tile data internally.

## Key Changes

### 1. Data Flow
**Before:**
- MahjongUIManager stored `vector<TileType> playerHand` and `vector<TileStats> tileStats`
- UIManager methods called without parameters
- RewardBriefcase stored `vector<RewardTile> rewardTiles`

**After:**
- Inventory holds `vector<Tile> tiles` where each Tile has `TileType type` and `TileStats stat`
- Me (player) has `Inventory hand`
- RewardBriefcase has `Inventory &inventory` (reference)
- UI methods take Inventory references as parameters

### 2. Method Signatures Changed

#### MahjongUIManager
```cpp
// OLD
void update(UpdateContext& uc);
void draw();
TileType getSelectedTile();
void nextTile();
void previousTile();
void selectTileByType(TileType type);
const vector<TileType>& getPlayerHand();

// NEW
void createHandUI(Inventory &inventory, int screenWidth, int screenHeight);
void update(Inventory &inventory);
void draw();
TileType getSelectedTile(Inventory &inventory);
void nextTile(Inventory &inventory);
void previousTile(Inventory &inventory);
void selectTileByType(Inventory &inventory, TileType type);
// Removed: getPlayerHand, getTileStats, setTileStats, setTileType
```

#### UIManager
```cpp
// OLD
void update();
void draw(UpdateContext& uc);
void updatePauseMenu();
void drawPauseMenu();
void updateBriefcaseUI(RewardBriefcase *briefcase);
void drawBriefcaseUI(RewardBriefcase *briefcase);

// NEW
void update(Inventory &playerInventory);
void draw(UpdateContext& uc, Inventory &playerInventory);
void updatePauseMenu(Inventory &playerInventory);
void drawPauseMenu(Inventory &playerInventory);
void updateBriefcaseUI(Inventory &briefcaseInventory, Inventory &playerInventory);
void drawBriefcaseUI(Inventory &briefcaseInventory, Inventory &playerInventory);
```

#### RewardBriefcase
```cpp
// OLD
RewardBriefcase(const Vector3 &position, const vector<RewardTile> &tiles);
vector<RewardTile>& GetRewardTiles();

// NEW
RewardBriefcase(const Vector3 &position, Inventory &inventory);
Inventory& GetInventory();
```

### 3. Implementation Steps

#### Step 1: Update uiManager.cpp
In every method that currently accesses:
- `muim.getPlayerHand()` → pass `playerInventory` to method and use `playerInventory.getTiles()`
- `muim.getTileStats(i)` → use `playerInventory.getTiles()[i].stat`
- `muim.setTileStats(i, stats)` → use `playerInventory.getTiles()[i].stat = stats`
- `muim.setTileType(i, type)` → use `playerInventory.getTiles()[i].type = type`

Key methods to update:
1. `UIManager::update(Inventory &playerInventory)`
   - Call `muim.createHandUI(playerInventory, screenW, screenH)` first
   - Call `muim.update(playerInventory)`

2. `UIManager::draw(UpdateContext& uc, Inventory &playerInventory)`
   - Pass inventory through as needed
   - Call `muim.draw()` (no params)

3. `UIManager::updatePauseMenu(Inventory &playerInventory)`
   - Replace all `muim.getPlayerHand()` with `playerInventory.getTiles()`
   - Replace `muim.getTileStats(i)` with `playerInventory.getTiles()[i].stat`

4. `UIManager::drawPauseMenu(Inventory &playerInventory)`
   - Replace `muim.getPlayerHand()` with `playerInventory.getTiles()`
   - Replace `muim.getTileStats(i)` with `playerInventory.getTiles()[i].stat`

5. `UIManager::updateBriefcaseUI(Inventory &briefcaseInv, Inventory &playerInv)`
   - Replace `briefcase->GetRewardTiles()` with `briefcaseInv.getTiles()`
   - Replace `muim.getTileStats(i)` with `playerInv.getTiles()[i].stat`
   - Swap logic: `swap(briefcaseInv.getTiles()[i], playerInv.getTiles()[j])`

6. `UIManager::drawBriefcaseUI(Inventory &briefcaseInv, Inventory &playerInv)`
   - Similar replacements as updateBriefcaseUI

#### Step 2: Update main.cpp
```cpp
// OLD
uiManager.update();
uiManager.draw(uc);
uiManager.updateBriefcaseUI(briefcase);
uiManager.drawBriefcaseUI(briefcase);

// NEW
uiManager.update(player.hand);
uiManager.draw(uc, player.hand);
if (briefcase)
{
    uiManager.updateBriefcaseUI(briefcase->GetInventory(), player.hand);
    uiManager.drawBriefcaseUI(briefcase->GetInventory(), player.hand);
}
```

#### Step 3: Update scene.cpp
Where briefcases are spawned:
```cpp
// Create inventory for briefcase
Inventory* briefcaseInv = new Inventory();
// Populate with reward tiles
briefcaseInv->getTiles().push_back(Tile(TileStats(...), TileType::...));
// Create briefcase with inventory reference
auto* briefcase = new RewardBriefcase(position, *briefcaseInv);
```

#### Step 4: Update rewardBriefcase.cpp
```cpp
// Constructor
RewardBriefcase::RewardBriefcase(const Vector3 &pos, Inventory &inv)
    : position(pos), inventory(inv)
{
    // ...
}
```

### 4. Benefits
- Single source of truth for tile data (Inventory)
- UI becomes pure view/controller - doesn't own data
- Easier to serialize/save game state
- Cleaner separation of concerns
- Player and briefcase use same Inventory interface

### 5. Testing Checklist
- [ ] Pause menu displays player hand correctly
- [ ] Can select tiles in pause menu
- [ ] Can drag tiles to slots
- [ ] Stats display shows correct values
- [ ] Briefcase UI opens and displays tiles
- [ ] Can swap tiles between briefcase and hand
- [ ] Tile types and stats preserved after swap
- [ ] No crashes when opening/closing UIs

## Current Status
- [x] Updated MahjongUIManager interface and implementation
- [x] Updated UIManager header with new signatures
- [x] Updated RewardBriefcase header
- [ ] Update UIManager implementation (uiManager.cpp)
- [ ] Update main.cpp to pass inventories
- [ ] Update scene.cpp briefcase spawning
- [ ] Update rewardBriefcase.cpp constructor
- [ ] Build and test

## Next Steps
1. Complete uiManager.cpp refactoring (largest file)
2. Update main.cpp game loop
3. Update scene briefcase spawning
4. Test each UI screen
