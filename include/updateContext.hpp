#pragma once

// Forward declarations to avoid circular includes
class Scene;
class Me;
class UIManager; // added forward declaration

/**
 * @brief Snapshot of player input for a single frame.
 *
 * `side` and `forward` use small integer values (-1/0/1) to represent
 * strafe/forward movement. `jumpPressed` and `crouchHold` are booleans.
 */
typedef struct PlayerInput
{
    char side;
    char forward;
    bool jumpPressed;
    bool crouchHold;
    PlayerInput(char _side, char _forward, bool _jumpPressed, bool _crouchHold) : side(_side), forward(_forward), jumpPressed(_jumpPressed), crouchHold(_crouchHold) {}
} PlayerInput;

/**
 * @brief Context object passed into Update() functions each frame.
 *
 * The `UpdateContext` aggregates references to the `Scene`, the `player`
 * entity, the current `PlayerInput` snapshot and an optional `UIManager`
 * pointer for systems that require UI state (selected tile, textures).
 *
 * Construct one per frame in `main()` and pass by reference to scene,
 * entity and manager update methods.
 */
typedef struct UpdateContext
{
    Scene *const scene;
    Me *const player;
    PlayerInput playerInput;
    UIManager *uiManager; // pointer to UI manager so systems can access UI state (selected tile / textures)

    UpdateContext(Scene *s, Me *p, PlayerInput pi, UIManager *ui = nullptr) : scene(s), player(p), playerInput(pi), uiManager(ui) {}
} UpdateContext;