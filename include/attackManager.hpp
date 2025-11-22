#pragma once
#include "attack.hpp"
#include <vector>
#include "object.hpp"
#include "uiManager.hpp"

// Manages all attacks in the game, instance hold by scene
class AttackManager
{
private:
    std::vector<ThousandAttack *> thousandAttack; // List of ThousandAttack instances
    std::vector<TripletAttack *> tripletAttack;
    std::vector<SingleTileAttack *> singleTileAttack;
    std::vector<std::pair<MahjongTileType, Rectangle>> thrownTiles; // History of thrown tiles and their texture rects

    void checkActivation(Entity *player);

public:
    ~AttackManager(); // Destructor to clean up dynamically allocated attacks

    // Updates all attacks managed by the AttackManager
    void update();

    void recordThrow(MahjongTileType tile, Entity* player, Texture2D* texture, Rectangle tile_rect);

    // Retrieves or creates a ThousandAttack for the given entity
    ThousandAttack *getThousandAttack(Entity *spawnedBy);
    TripletAttack *getTripletAttack(Entity *spawnedBy);
    SingleTileAttack *getSingleTileAttack(Entity *spawnedBy);

    // Returns a list of objects representing all projectiles for rendering or collision detection
    std::vector< Object *> getObjects() const;
};
