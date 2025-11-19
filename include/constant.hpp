#pragma once
// Movement constants
#define GRAVITY 32.0f
#define MAX_SPEED 20.0f
#define CROUCH_SPEED 5.0f
#define JUMP_FORCE 12.0f
#define MAX_ACCEL 150.0f
// Grounded drag
#define FRICTION 0.86f
#define PROJECTILE_FRICTION 0.97f
// Increasing air drag, increases strafing speed
#define AIR_DRAG 0.98f
#define PROJECTILE_AIR_DRAG 0.99f
// Responsiveness for turning movement direction to looked direction
#define CONTROL 15.0f
#define CROUCH_HEIGHT 0.0f
#define STAND_HEIGHT 1.0f
#define BOTTOM_HEIGHT 0.5f

#define MAX_HEALTH_ME 100
#define MAX_HEALTH_ENEMY 100

#define TOWER_COLOR {150, 200, 200, 255}