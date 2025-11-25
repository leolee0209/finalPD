#pragma once
#include "attack.hpp"
#include <vector>
#include "object.hpp"
#include "uiManager.hpp"
#include "updateContext.hpp"

/**
 * @brief Manages all attack controllers and tracks thrown tiles for combos.
 *
 * AttackManager owns per-spawner `ThousandTileAttack` instances and records
 * recent thrown tiles to detect combo activations (thousand/triplet).
 */
class AttackManager
{
private:
    //std::vector<ThousandAttack *> thousandAttack; // List of ThousandAttack instances
    //std::vector<TripletAttack *> tripletAttack;
    std::vector<ThousandTileAttack *> singleTileAttack;
    std::vector<std::pair<MahjongTileType, Rectangle>> thrownTiles; // History of thrown tiles and their texture rects

    void checkActivation(Entity *player);

public:
    ~AttackManager(); // Destructor to clean up dynamically allocated attacks

    /**
     * @brief Update all managed attacks (call each frame).
     */
    void update(UpdateContext& uc);

    /**
     * @brief Record a thrown tile into history for combo detection.
     */
    void recordThrow(UpdateContext &uc);

    /**
     * @brief Retrieve or create a `ThousandTileAttack` bound to `spawnedBy`.
     */
    ThousandTileAttack *getSingleTileAttack(Entity *spawnedBy);

    /**
     * @brief Get all entity pointers managed by attacks (projectiles).
     */
    std::vector<Entity *> getEntities();

    /**
     * @brief Get objects representing all projectiles/connectors for rendering/collision.
     */
    std::vector< Object *> getObjects() const;
};
