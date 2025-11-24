#pragma once

// Forward declarations to avoid circular includes
class Scene;
class Me;
typedef struct PlayerInput
{
    char side;
    char forward;
    bool jumpPressed;
    bool crouchHold;
    PlayerInput(char _side, char _forward, bool _jumpPressed, bool _crouchHold) : side(_side), forward(_forward), jumpPressed(_jumpPressed), crouchHold(_crouchHold) {}
} PlayerInput;

typedef struct UpdateContext
{
    Scene *const scene;
    Me *const player;
    PlayerInput playerInput;

    UpdateContext(Scene *s, Me *p, PlayerInput pi) : scene(s), player(p), playerInput(pi) {}
} UpdateContext;