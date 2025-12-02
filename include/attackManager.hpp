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
    enum class SlotAttackKind
    {
        None,
        DefaultThrow,
        DotBomb,
        Melee,
        Dash
    };
    //std::vector<ThousandAttack *> thousandAttack; // List of ThousandAttack instances
    //std::vector<TripletAttack *> tripletAttack;
    std::vector<BasicTileAttack *> basicTileAttacks;
    std::vector<ThousandTileAttack *> singleTileAttack;
    std::vector<MeleePushAttack *> meleeAttacks;
    std::vector<DashAttack *> dashAttacks;
    std::vector<DotBombAttack *> dotBombAttacks;
    std::vector<std::pair<MahjongTileType, Rectangle>> thrownTiles; // History of thrown tiles and their texture rects
    AttackController *attackLockOwner = nullptr;

    void checkActivation(Entity *player);
    SlotAttackKind classifySlotAttack(const std::vector<SlotTileEntry> &slotEntries) const;
    float computeSlotCooldownPercent(int slotIndex, UpdateContext &uc);

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
     * @brief Retrieve or create a `BasicTileAttack` bound to `spawnedBy`.
     */
    BasicTileAttack *getBasicTileAttack(Entity *spawnedBy);

    /**
     * @brief Retrieve or create a `ThousandTileAttack` bound to `spawnedBy`.
     */
    ThousandTileAttack *getSingleTileAttack(Entity *spawnedBy);

    /**
     * @brief Retrieve or create a melee push attack controller for `spawnedBy`.
     */
    MeleePushAttack *getMeleeAttack(Entity *spawnedBy);
    DashAttack *getDashAttack(Entity *spawnedBy);
    DotBombAttack *getDotBombAttack(Entity *spawnedBy);
    bool triggerSlotAttack(int slotIndex, UpdateContext &uc);

    /**
     * @brief Get all entity pointers managed by attacks (projectiles).
     */
    std::vector<Entity *> getEntities(EntityCategory cat = ENTITY_PROJECTILE);

    /**
     * @brief Get objects representing all projectiles/connectors for rendering/collision.
     */
    std::vector< Object *> getObjects() const;

    bool isAttackLockedByOther(const AttackController *controller) const;
    bool tryLockAttack(AttackController *controller);
    void releaseAttackLock(const AttackController *controller);
};
