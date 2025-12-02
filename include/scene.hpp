#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <raylib.h>
#include "attackManager.hpp"
#include "updateContext.hpp"
#include "enemyManager.hpp"
#include "collidableModel.hpp"

class Object;

/**
 * @brief Represents the 3D world and manages objects, entities and attacks.
 *
 * Scene owns static objects, a floor Object, an AttackManager and an
 * EnemyManager. It provides `DrawScene()` to render the world (call inside a
 * camera Begin/End) and `Update(UpdateContext&)` to advance game simulation
 * each frame.
 */
class Scene
{
private:
    std::vector<Object *> objects; // List of static objects in the scene (e.g., towers, obstacles)
    Object floor;                  // Represents the floor of the scene
    Texture2D wallTexture{};       // Procedural room walls texture
    Texture2D floorTexture{};      // Procedural room floor texture
    Shader lightingShader{};       // Shared lighting shader
    int ambientLoc = -1;
    int viewPosLoc = -1;
    Vector4 ambientColor = {0.12f, 0.09f, 0.08f, 1.0f};
    Vector3 shaderViewPos = {0.0f, 6.0f, 6.0f};
    Color skyColor = {12, 17, 32, 255};
    Color shadowColor = {0, 0, 0, 80};
    float shadowThickness = 0.05f;
    float shadowInflation = 1.12f;
    float shadowMinAlpha = 0.2f;

    struct CachedModel
    {
        Model model;
        int refCount = 0;
    };

    std::vector<std::unique_ptr<CollidableModel>> decorations;
    std::unordered_map<std::string, CachedModel> decorationModelCache;
    std::unique_ptr<btDefaultCollisionConfiguration> bulletConfig;
    std::unique_ptr<btCollisionDispatcher> bulletDispatcher;
    std::unique_ptr<btBroadphaseInterface> bulletBroadphase;
    std::unique_ptr<btCollisionWorld> bulletWorld;

    struct DoorLeafVisual
    {
        BoundingBox bounds{};
        Vector3 hingeLocal{};
        float targetAngleDeg = 90.0f;
        float currentAngleDeg = 0.0f;
        int meshIndex = -1;
        bool valid = false;
    };

    struct DoorInstance
    {
        std::unique_ptr<CollidableModel> collider;
        bool collisionEnabled = false;
        bool opening = false;
        bool openComplete = false;
        float openProgress = 0.0f;
        float openDuration = 1.2f;
        Vector3 basePosition{};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        Vector3 rotationAxis{0.0f, 1.0f, 0.0f};
        float rotationAngleDeg = 0.0f;
        Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
        Model *visualModel = nullptr; // Non-owning: model owned by decoration cache
        DoorLeafVisual leftLeaf;
        DoorLeafVisual rightLeaf;
    };

    struct Room
    {
        std::string name;
        BoundingBox bounds{};
        std::vector<int> connectedDoors;
        bool hadEnemies = false;
        bool completed = false;
    };

    std::vector<Room> rooms;
    std::vector<DoorInstance> doors;

    // Helper function to draw a 3D rectangle (cube) for an object
    void DrawRectangle(const Object &o) const;
    void DrawSphereObject(const Object &o) const;
    void DrawCubeTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color) const;                      // Draw cube textured
    void DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position, float width, float height, float length, Color color) const; // Draw cube with a region of a texture
    void DrawTexturedSphere(Texture2D &texture, const Rectangle &source, const Vector3 &position, float radius, Color tint) const;
    void ApplyFullTexture(Object &obj, Texture2D &texture);
    void InitializeLighting();
    void ShutdownLighting();
    void CreatePointLight(Vector3 position, Color color, float intensity = 1.0f);
    void DrawPlanarShadow(const Object &o, float floorY) const;
    void DrawShadowCollection(const std::vector<Object *> &items, float floorY) const;
    float GetFloorTop() const;
    CollidableModel *AddDecoration(const char *modelPath,
                                   Vector3 desiredPosition,
                                   float targetHeight,
                                   float rotationYDeg = 0.0f,
                                   bool addCollision = false);
    void ConfigureDoorPlacement(CollidableModel *door, const Vector3 &desiredPosition);
    void InitializeRooms(float roomWidth, float roomLength, float wallHeight,
                         const Vector3 &firstCenter, const Vector3 &secondCenter);
    void DrawDecorations() const;
    Model *AcquireDecorationModel(const std::string &relativePath);
    void ReleaseDecorationModels();
    void InitializeBulletWorld();
    void ShutdownBulletWorld();
    void RemoveDecorationColliders();
    void AppendDecorationCollisions(const Object &obj, std::vector<CollisionResult> &out) const;
    static btTransform BuildBtTransform(const Object &obj);
    static btCollisionShape *CreateShapeFromObject(const Object &obj);
    static RenderTexture2D CreateHealthBarTexture(int currentHealth, int maxHealth, float fillPercent);
    void UpdateRooms();
    void OnRoomCompleted(int roomIndex);
    void OpenDoor(int doorIndex);
    void UpdateDoorAnimations(float deltaSeconds);
    void DrawDoors() const;
    bool InitializeDoorVisuals(DoorInstance &door);
    void ApplyLightingToDoor(DoorInstance &door);
    Vector3 TransformDoorPoint(const DoorInstance &door, const Vector3 &localPoint) const;
    void DrawDoorLeaf(const DoorInstance &door, const DoorLeafVisual &leaf) const;
    void ShutdownDoorVisuals();
    std::unique_ptr<CollidableModel> DetachDecoration(CollidableModel *target);
public:
    AttackManager am; // Manages all attacks in the scene
    EnemyManager em;
    Model cubeModel; // Shared cube model used to render rotated cubes
    Model sphereModel; // Shared sphere model used to render spheres
    Texture2D glowTexture{}; // Texture for the glow effect on bullets

    /**
     * @brief Destructor will release GPU resources (model) and deallocate owned data.
     */
    ~Scene();

    /**
     * @brief Render the full scene, objects, entities and attacks.
     *
     * Call while a 3D camera block is active.
     */
    void DrawScene(Camera camera) const;
    void DrawEnemyHealthDialogs(const Camera &camera) const;

    /**
     * @brief Advance scene simulation: update enemies, attacks and other systems.
     *
     * @param uc Frame update context (scene, player, input snapshot).
     */
    void Update(UpdateContext &uc);

    /**
     * @brief Construct a new Scene with default objects and managers.
     */
    Scene();

    /**
     * @brief Return the vector of static objects placed in the scene.
     */
    std::vector<Object *> getStaticObjects() const;
    void CollectDecorationCollisions(const Object &obj, std::vector<CollisionResult> &out) const { this->AppendDecorationCollisions(obj, out); }
    bool CheckDecorationCollision(const Object &obj) const;
    bool CheckDecorationSweep(const Vector3 &start, const Vector3 &end, float radius) const;

    /**
     * @brief Return a list of entity pointers currently in the scene.
     */
    std::vector<Entity *> getEntities(EntityCategory cat = ENTITY_ALL);
    void SetViewPosition(const Vector3 &viewPosition);
    Color getSkyColor() const { return this->skyColor; }
};