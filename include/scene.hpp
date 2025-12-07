#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <map>
#include <raylib.h>
#include "attackManager.hpp"
#include "updateContext.hpp"
#include "enemyManager.hpp"
#include "collidableModel.hpp"
#include "tileModelManager.hpp"
#include "rewardBriefcase.hpp"
#include "particle.hpp"
#include "tileObject.hpp"

struct DamageIndicator
{
    Vector3 worldPosition{0.0f, 0.0f, 0.0f};
    Vector2 screenOffset{0.0f, 0.0f};
    Vector2 velocity{0.0f, 0.0f};
    float age = 0.0f;
    float lifetime = 0.85f;
    std::string text;
};

class DamageIndicatorSystem
{
public:
    void Spawn(const Vector3 &worldPosition, float amount);
    void Update(float deltaSeconds);
    void Draw(const Camera &camera) const;
    void Clear();

private:
    mutable std::vector<DamageIndicator> indicators;
};

class Object;

struct LightConfig {
    Vector3 direction = {0.5f, -1.0f, 0.5f};
    Color color = WHITE;
    float opacity = 0.5f;
};

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
    Shader lightingShader{};       // Shared lighting shader
    int ambientLoc = -1;
    int viewPosLoc = -1;
    // Evening/Night ambient color (darker, bluer)
    Vector4 ambientColor = {0.4f, 0.4f, 0.45f, 1.0f}; 
    Vector3 shaderViewPos = {0.0f, 6.0f, 6.0f};
    // Morning sky color (Sky Blue)
    Color skyColor = {135, 206, 235, 255}; 
    Vector3 playerSpawnPosition = {0.0f, 0.0f, 0.0f};

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
    
    // Bullet resources for static objects
    std::vector<btCollisionObject*> staticColliders;
    std::vector<btCollisionShape*> staticShapes;

    std::vector<std::unique_ptr<RewardBriefcase>> rewardBriefcases;
    std::vector<std::unique_ptr<TileObject>> tileObjects;
    std::map<TileType, std::vector<Matrix>> wallInstances;
    DamageIndicatorSystem damageIndicators;
    TileModelManager tileModelManager;

    // Shadow mapping
    RenderTexture2D shadowMapStatic{};
    RenderTexture2D shadowMapDynamic{};
    Shader shadowShader{};
    Matrix lightView{};
    Matrix lightProj{};
    bool staticShadowsRendered = false;
    
    void DrawStaticObjects(Shader shader) const;
    void DrawDynamicObjects(Shader shader) const;

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
    float GetFloorTop() const;
    CollidableModel *AddDecoration(const char *modelPath,
                                   Vector3 desiredPosition,
                                   float targetHeight,
                                   float rotationYDeg = 0.0f,
                                   bool addCollision = false);
    void DrawDecorations() const;
    Model *AcquireDecorationModel(const std::string &relativePath);
    void ReleaseDecorationModels();
    void InitializeBulletWorld();
    void ShutdownBulletWorld();
    void RemoveDecorationColliders();
    void AppendDecorationCollisions(const Object &obj, std::vector<CollisionResult> &out) const;
    static btTransform BuildBtTransform(const Object &obj);
    static btCollisionShape *CreateShapeFromObject(const Object &obj);
    std::unique_ptr<CollidableModel> DetachDecoration(CollidableModel *target);
    
public:
    LightConfig lightConfig;
    void UpdateLighting();
    void RenderShadows();

    AttackManager am; // Manages all attacks in the scene
    EnemyManager em;
    ParticleSystem particles; // Particle system for visual effects
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
    bool CheckLineOfSight(const Vector3 &start, const Vector3 &end) const;

    /**
     * @brief Return a list of entity pointers currently in the scene.
     */
    std::vector<Entity *> getEntities(EntityCategory cat = ENTITY_ALL);
    void SetViewPosition(const Vector3 &viewPosition);
    Color getSkyColor() const { return this->skyColor; }
    
    void SetPlayerSpawnPosition(Vector3 pos) { this->playerSpawnPosition = pos; }
    Vector3 GetPlayerSpawnPosition() const { return this->playerSpawnPosition; }
        void EmitDamageIndicator(const Enemy &enemy, float damageAmount);
        void DrawDamageIndicators(const Camera &camera) const;
        void DrawEnemyHealthDialogs(const Camera &camera) const;
    
        std::vector<RewardBriefcase *> GetRewardBriefcases();
            
        void AddTileObject(TileType type, Vector3 position, Quaternion rotation, float scaleFactor = 1.0f);
        void AddWallTile(TileType type, Vector3 position, Quaternion rotation, float scaleFactor = 1.0f);
        void AddWallInstance(TileType type, Vector3 position, Quaternion rotation, Vector3 scale);
        void AddStaticObject(Object* obj);
    };
    