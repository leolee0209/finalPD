#pragma once
#include <vector>
#include <raylib.h>
#include "attackManager.hpp"
#include "updateContext.hpp"
#include "enemyManager.hpp"

class Object;
// The Scene class represents the game world, including entities, objects, and attacks.
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

    // Destructor (will unload model in implementation and deallocate entities)
    ~Scene();
    // Draws the entire scene, including objects, entities, and attacks
    void DrawScene() const;
    // Updates all entities and attacks in the scene
    void Update(UpdateContext &uc);
    // Constructor initializes the scene with default objects
    Scene();
    // Getter for the list of objects in the scene
    std::vector<Object *> getStaticObjects() const;
    std::vector<Entity *> getEntities();
};