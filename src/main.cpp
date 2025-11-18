#include "raylib.h"
#include "raymath.h"
#include "constant.hpp"
#include "me.hpp"
#include "scene.hpp"
//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static Vector2 sensitivity = {0.001f, 0.001f};

static Me player;
static Scene scene;
static Vector2 lookRotation = {0};
static float headTimer = 0.0f;
static float walkLerp = 0.0f;
static float headLerp = STAND_HEIGHT;
static Vector2 lean = {0};

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateCameraFPS(Camera *camera);

int main(void)
{
    // Initialization-----------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - 3d camera fps");

    // Initialize camera variables
    // NOTE: UpdateCameraFPS() takes care of the rest
    Camera camera = {0};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position = {
        player.px(),
        player.py() + (BOTTOM_HEIGHT + headLerp),
        player.pz(),
    };
    UpdateCameraFPS(&camera); // Update camera parameters
    DisableCursor();          // Limit cursor to relative movement inside the window
    SetTargetFPS(60);         // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        Vector2 mouseDelta = GetMouseDelta();
        lookRotation.x -= mouseDelta.x * sensitivity.x;
        lookRotation.y += mouseDelta.y * sensitivity.y;

        char sideway = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
        char forward = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
        bool crouching = IsKeyDown(KEY_LEFT_CONTROL);
        player.UpdateBody(lookRotation.x, sideway, forward, IsKeyPressed(KEY_SPACE), crouching, scene.collided(player));

        float delta = GetFrameTime();
        headLerp = Lerp(headLerp, (crouching ? CROUCH_HEIGHT : STAND_HEIGHT), 20.0f * delta);
        camera.position = {
            player.px(),
            player.py() + (BOTTOM_HEIGHT + headLerp),
            player.pz(),
        };

        if (player.isGrounded() && ((forward != 0) || (sideway != 0)))
        {
            headTimer += delta * 3.0f;
            walkLerp = Lerp(walkLerp, 1.0f, 10.0f * delta);
            camera.fovy = Lerp(camera.fovy, 55.0f, 5.0f * delta);
        }
        else
        {
            walkLerp = Lerp(walkLerp, 0.0f, 10.0f * delta);
            camera.fovy = Lerp(camera.fovy, 60.0f, 5.0f * delta);
        }

        lean.x = Lerp(lean.x, sideway * 0.02f, 10.0f * delta);
        lean.y = Lerp(lean.y, forward * 0.015f, 10.0f * delta);

        UpdateCameraFPS(&camera);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
        scene.DrawScene();
        EndMode3D();

        // Draw info box
        DrawRectangle(5, 5, 330, 75, Fade(SKYBLUE, 0.5f));
        DrawRectangleLines(5, 5, 330, 75, BLUE);
        DrawText("Camera controls:", 15, 15, 10, BLACK);
        DrawText("hello123", 15, 30, 10, BLACK);
        DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);
        //DrawText(TextFormat("- Velocity Len: (%06.3f)", Vector2Length({player.velocity.x, player.velocity.z})), 15, 60, 10, BLACK);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Update camera for FPS behaviour
static void UpdateCameraFPS(Camera *camera)
{
    const Vector3 up = {0.0f, 1.0f, 0.0f};
    const Vector3 targetOffset = {0.0f, 0.0f, -1.0f};

    // Left and right
    Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, lookRotation.x);

    // Clamp view up
    float maxAngleUp = Vector3Angle(up, yaw);
    maxAngleUp -= 0.001f; // Avoid numerical errors
    if (-(lookRotation.y) > maxAngleUp)
    {
        lookRotation.y = -maxAngleUp;
    }

    // Clamp view down
    float maxAngleDown = Vector3Angle(Vector3Negate(up), yaw);
    maxAngleDown *= -1.0f;  // Downwards angle is negative
    maxAngleDown += 0.001f; // Avoid numerical errors
    if (-(lookRotation.y) < maxAngleDown)
    {
        lookRotation.y = -maxAngleDown;
    }

    // Up and down
    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

    // Rotate view vector around right axis
    float pitchAngle = -lookRotation.y - lean.y;
    pitchAngle = Clamp(pitchAngle, -PI / 2 + 0.0001f, PI / 2 - 0.0001f); // Clamp angle so it doesn't go past straight up or straight down
    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    // Head animation
    // Rotate up direction around forward axis
    float headSin = sinf(headTimer * PI);
    float headCos = cosf(headTimer * PI);
    const float stepRotation = 0.01f;
    camera->up = Vector3RotateByAxisAngle(up, pitch, headSin * stepRotation + lean.x);

    // Camera BOB
    const float bobSide = 0.1f;
    const float bobUp = 0.15f;
    Vector3 bobbing = Vector3Scale(right, headSin * bobSide);
    bobbing.y = fabsf(headCos * bobUp);

    camera->position = Vector3Add(camera->position, Vector3Scale(bobbing, walkLerp));
    camera->target = Vector3Add(camera->position, pitch);
}