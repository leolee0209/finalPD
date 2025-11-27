#pragma once
#include <vector>
#include <raylib.h>
#include "attackManager.hpp"
#include "updateContext.hpp"
#include "enemyManager.hpp"

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

    // Helper function to draw a 3D rectangle (cube) for an object
    void DrawRectangle(Object &o) const;
    void DrawCubeTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color) const;                      // Draw cube textured
    void DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position, float width, float height, float length, Color color) const; // Draw cube with a region of a texture
public:
    AttackManager am; // Manages all attacks in the scene
    EnemyManager em;
    Model cubeModel; // Shared cube model used to render rotated cubes

    /**
     * @brief Destructor will release GPU resources (model) and deallocate owned data.
     */
    ~Scene();

    /**
     * @brief Render the full scene, objects, entities and attacks.
     *
     * Call while a 3D camera block is active.
     */
    void DrawScene() const;

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

    /**
     * @brief Return a list of entity pointers currently in the scene.
     */
    std::vector<Entity *> getEntities(EntityCategory cat = ENTITY_ALL);
};