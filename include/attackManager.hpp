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
        Dash,
        BambooTriple,
        FanShot,
        SeismicSlam,
        GravityWell,
        ChainLightning,
        OrbitalShield
    };
    //std::vector<ThousandAttack *> thousandAttack; // List of ThousandAttack instances
    //std::vector<TripletAttack *> tripletAttack;
    std::vector<BambooBasicAttack *> basicTileAttacks;
    std::vector<MeleePushAttack *> meleeAttacks;
    std::vector<DashAttack *> dashAttacks;
    std::vector<BambooBombAttack *> bambooBombAttacks;
    std::vector<BambooBasicBuffAttack *> bambooTripleAttacks;
    std::vector<DragonClawAttack *> dragonClawAttacks;
    std::vector<ArcaneOrbAttack *> arcaneOrbAttacks;
    std::vector<FanShotAttack *> fanShotAttacks;
    std::vector<SeismicSlamAttack *> seismicSlamAttacks;
    std::vector<GravityWellAttack *> gravityWellAttacks;
    std::vector<ChainLightningAttack *> chainLightningAttacks;
    std::vector<OrbitalShieldAttack *> orbitalShieldAttacks;
    std::vector<std::pair<TileType, Rectangle>> thrownTiles; // History of thrown tiles and their texture rects
    AttackController *attackLockOwner = nullptr;

    SlotAttackKind classifySlotAttack(const std::vector<SlotTileEntry> &slotEntries) const;
    float computeSlotCooldownPercent(int slotIndex, UpdateContext &uc);

public:
    /**
     * @brief Classify attack type from tile combination and return as string.
     * @param tiles Vector of tiles in the slot (order-independent for sequences).
     * @return String name of attack type, or "NA" if no valid attack.
     */
    static std::string classifyAttackType(const std::vector<SlotTileEntry> &tiles);

private:

public:
    ~AttackManager(); // Destructor to clean up dynamically allocated attacks

    /**
     * @brief Update all managed attacks (call each frame).
     */
    void update(UpdateContext& uc);

    /**
     * @brief Retrieve or create a `BambooBasicAttack` bound to `spawnedBy`.
     */
    BambooBasicAttack *getBasicTileAttack(Entity *spawnedBy);

    /**
     * @brief Retrieve or create a melee push attack controller for `spawnedBy`.
     */
    MeleePushAttack *getMeleePushAttack(Entity *spawnedBy);
    DashAttack *getDashAttack(Entity *spawnedBy);
    BambooBombAttack *getBambooBombAttack(Entity *spawnedBy);
    BambooBasicBuffAttack *getBambooTripleAttack(Entity *spawnedBy);
    DragonClawAttack *getDragonClawAttack(Entity *spawnedBy);
    ArcaneOrbAttack *getArcaneOrbAttack(Entity *spawnedBy);
    FanShotAttack *getFanShotAttack(Entity *spawnedBy);
    SeismicSlamAttack *getSeismicSlamAttack(Entity *spawnedBy);
    GravityWellAttack *getGravityWellAttack(Entity *spawnedBy);
    ChainLightningAttack *getChainLightningAttack(Entity *spawnedBy);
    OrbitalShieldAttack *getOrbitalShieldAttack(Entity *spawnedBy);
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
