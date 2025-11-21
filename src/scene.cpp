#include "scene.hpp"
#include <raylib.h>
#include "constant.hpp"

// Destructor: unload shared model resources
Scene::~Scene()
{
    // Only unload GPU resources if the window/context is still active.
    if (IsWindowReady())
    {
        UnloadModel(this->cubeModel);
    }
}

// Draws a 3D rectangle (cube) for the given object
void Scene::DrawRectangle(const Object &o) const
{
    // Draw the object using a shared cube model scaled to the object's size.
    // DrawModelEx handles translation, rotation (axis + angle) and scale.
    DrawModelEx(this->cubeModel, o.getPos(), o.getRotationAxis(), o.getRotationAngle(), o.getSize(), TOWER_COLOR);

    // Draw wireframe using extended function to match rotation and scale
    DrawModelWiresEx(this->cubeModel, o.getPos(), o.getRotationAxis(), o.getRotationAngle(), o.getSize(), DARKBLUE);
}

// Adds an entity to the scene
void Scene::addEntity(Entity e)
{
    this->entities.push_back(e); // Add the entity to the list of entities
}

// Draws the entire scene, including the floor, objects, entities, and attacks
void Scene::DrawScene() const
{
    const int floorExtent = 25;       // Extent of the floor in both x and z directions
    const float tileSize = 5.0f;      // Size of each floor tile
    const Color tileColor1 = {150, 200, 200, 255}; // Primary tile color

    // Draw the floor as a grid of tiles
    for (int y = -floorExtent; y < floorExtent; y++)
    {
        for (int x = -floorExtent; x < floorExtent; x++)
        {
            if ((y & 1) && (x & 1)) // Alternate tile pattern
            {
                DrawPlane({x * tileSize, 0.0f, y * tileSize}, {tileSize, tileSize}, tileColor1);
            }
            else if (!(y & 1) && !(x & 1))
            {
                DrawPlane({x * tileSize, 0.0f, y * tileSize}, {tileSize, tileSize}, LIGHTGRAY);
            }
        }
    }

    // Draw all static objects in the scene
    for (const auto &o : this->objects)
    {
        DrawRectangle(o);
    }

    // Draw all entities in the scene
    for (const auto &o : this->entities)
    {
        DrawRectangle(o.obj());
    }

    // Draw all projectiles managed by the AttackManager
    for (const auto &o : this->am.getObjects())
    {
        DrawRectangle(*o);
    }

    // Draw a red sun in the sky
    DrawSphere({300.0f, 300.0f, 0.0f}, 100.0f, {255, 0, 0, 255});
}

// Updates all entities and attacks in the scene
void Scene::Update()
{
    // Update all entities in the scene
    for (auto &e : this->entities)
    {
        e.UpdateBody();
    }

    // Update all attacks managed by the AttackManager
    this->am.update();
}

// Constructor initializes the scene with default objects
Scene::Scene()
{
    const Vector3 towerSize = {16.0f, 32.0f, 16.0f}; // Size of the towers
    const Color towerColor = {150, 200, 200, 255};   // Color of the towers

    Vector3 towerPos = {16.0f, 16.0f, 16.0f}; // Initial position of the first tower

    // Add four towers to the scene in a symmetrical pattern
    this->objects.push_back(Object(towerSize, towerPos));
    towerPos.x *= -1;
    this->objects.push_back(Object(towerSize, towerPos));
    towerPos.z *= -1;
    this->objects.push_back(Object(towerSize, towerPos));
    towerPos.x *= -1;
    this->objects.push_back(Object(towerSize, towerPos));

    // Create a shared unit cube model (unit size) and store it for rendering rotated/scaled objects
    Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    this->cubeModel = LoadModelFromMesh(cubeMesh);
}
