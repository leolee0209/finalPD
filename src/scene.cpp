#include "scene.hpp"
#include <raylib.h>
#include "constant.hpp"
#include <rlgl.h>
#include "enemyManager.hpp"
#include <iostream>
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
void Scene::DrawRectangle(Object &o) const
{
    if (o.useTexture && o.texture != nullptr)
    {
        // Draw the object using a textured cube
        // We must manually apply the rotation matrix because DrawCubeTextureRec
        // calculates vertices in world space by default.

        rlPushMatrix();
        // 1. Translate to the object's position
        rlTranslatef(o.pos.x, o.pos.y, o.pos.z);

        // 2. Apply the object's rotation
        Vector3 axis;
        float angle;
        o.getRotationAxisAngle(axis, angle);
        rlRotatef(angle, axis.x, axis.y, axis.z);

        // 3. Draw the cube at the local origin (0,0,0)
        // We pass {0,0,0} as the position because we have already moved
        // the drawing context to the object's location using rlTranslatef.
        DrawCubeTextureRec(*o.texture, o.sourceRect, {0.0f, 0.0f, 0.0f}, o.size.x, o.size.y, o.size.z, WHITE);
        rlPopMatrix();
    }
    else
    {
        // Draw the object using a shared cube model scaled to the object's size.
        // DrawModelEx handles translation, rotation (axis + angle) and scale.
        Vector3 axis;
        float angle;
        o.getRotationAxisAngle(axis, angle);
        DrawModelEx(this->cubeModel, o.getPos(), axis, angle, o.getSize(), TOWER_COLOR);

        // Draw wireframe using extended function to match rotation and scale
        DrawModelWiresEx(this->cubeModel, o.getPos(), axis, angle, o.getSize(), DARKBLUE);
    }
}

// Draws the entire scene, including the floor, objects, entities, and attacks
void Scene::DrawScene() const
{
    const int floorExtent = 25;                    // Extent of the floor in both x and z directions
    const float tileSize = 5.0f;                   // Size of each floor tile
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
    for (auto &o : this->objects)
    {
        if (o && o->isVisible())
            DrawRectangle(*o);
    }

    // Draw all entities in the EnemyManager
    for (const auto &e : this->em.getObjects())
    {
        if (e && e->isVisible())
            DrawRectangle(*e);
    }

    // Draw all projectiles managed by the AttackManager
    for (const auto &o : this->am.getObjects())
    {
        if (o && o->isVisible())
            DrawRectangle(*o);
    }

    // Draw a red sun in the sky
    DrawSphere({300.0f, 300.0f, 0.0f}, 100.0f, {255, 0, 0, 255});
}

// Updates all entities and attacks in the scene
void Scene::Update(UpdateContext &uc)
{
    // Update all entities in the scene
    this->em.update(uc);

    // Update all attacks managed by the AttackManager
    this->am.update(uc);
}

// Constructor initializes the scene with default objects
Scene::Scene()
{
    const Vector3 towerSize = {16.0f, 32.0f, 16.0f}; // Size of the towers
    const Color towerColor = {150, 200, 200, 255};   // Color of the towers

    Vector3 towerPos = {16.0f, 16.0f, 16.0f}; // Initial position of the first tower

    // Add four towers to the scene in a symmetrical pattern
    this->objects.push_back(new Object(towerSize, towerPos));
    towerPos.x *= -1;
    this->objects.push_back(new Object(towerSize, towerPos));
    towerPos.z *= -1;
    this->objects.push_back(new Object(towerSize, towerPos));
    towerPos.x *= -1;
    this->objects.push_back(new Object(towerSize, towerPos));

    // Create a shared unit cube model (unit size) and store it for rendering rotated/scaled objects
    Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    this->cubeModel = LoadModelFromMesh(cubeMesh);
}

// Getter for the list of objects in the scene
std::vector<Object *> Scene::getStaticObjects() const
{
    return this->objects;
}

std::vector<Entity *> Scene::getEntities(EntityCategory cat)
{
    std::vector<Entity *> r;
    // Query attack manager for projectiles if requested
    auto amEntities = this->am.getEntities(cat);
    r.insert(r.end(), amEntities.begin(), amEntities.end());
    // Query enemy manager for enemies if requested
    auto emEntities = this->em.getEntities(cat);
    r.insert(r.end(), emEntities.begin(), emEntities.end());

    return r;
}

void Scene::DrawCubeTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color) const
{
    float x = position.x;
    float y = position.y;
    float z = position.z;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

    // Vertex data transformation can be defined with the commented lines,
    // but in this example we calculate the transformed vertex data directly when calling rlVertex3f()
    // rlPushMatrix();
    // NOTE: Transformation is applied in inverse order (scale -> rotate -> translate)
    // rlTranslatef(2.0f, 0.0f, 0.0f);
    // rlRotatef(45, 0, 1, 0);
    // rlScalef(2.0f, 2.0f, 2.0f);

    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    // Front Face
    rlNormal3f(0.0f, 0.0f, 1.0f); // Normal Pointing Towards Viewer
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2); // Top Left Of The Texture and Quad
    // Back Face
    rlNormal3f(0.0f, 0.0f, -1.0f); // Normal Pointing Away From Viewer
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2); // Bottom Left Of The Texture and Quad
    // Top Face
    rlNormal3f(0.0f, 1.0f, 0.0f); // Normal Pointing Up
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2); // Top Right Of The Texture and Quad
    // Bottom Face
    rlNormal3f(0.0f, -1.0f, 0.0f); // Normal Pointing Down
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2); // Bottom Right Of The Texture and Quad
    // Right face
    rlNormal3f(1.0f, 0.0f, 0.0f); // Normal Pointing Right
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2); // Top Left Of The Texture and Quad
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2); // Bottom Left Of The Texture and Quad
    // Left Face
    rlNormal3f(-1.0f, 0.0f, 0.0f); // Normal Pointing Left
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2); // Bottom Left Of The Texture and Quad
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2); // Bottom Right Of The Texture and Quad
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2); // Top Right Of The Texture and Quad
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2); // Top Left Of The Texture and Quad
    rlEnd();
    // rlPopMatrix();

    rlSetTexture(0);
}

// Draw cube with texture piece applied to all faces
void Scene::DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position, float width, float height, float length, Color color) const
{
    float x = position.x;
    float y = position.y;
    float z = position.z;
    float texWidth = (float)texture.width;
    float texHeight = (float)texture.height;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

    // We calculate the normalized texture coordinates for the desired texture-source-rectangle
    // It means converting from (tex.width, tex.height) coordinates to [0.0f, 1.0f] equivalent
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);

    // Front face
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);

    // Back face
    rlNormal3f(0.0f, 0.0f, -1.0f);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);

    // Top face
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);

    // Bottom face
    rlNormal3f(0.0f, -1.0f, 0.0f);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);

    // Right face
    rlNormal3f(1.0f, 0.0f, 0.0f);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z - length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x + width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x + width / 2, y - height / 2, z + length / 2);

    // Left face
    rlNormal3f(-1.0f, 0.0f, 0.0f);
    rlTexCoord2f(source.x / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z - length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, (source.y + source.height) / texHeight);
    rlVertex3f(x - width / 2, y - height / 2, z + length / 2);
    rlTexCoord2f((source.x + source.width) / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z + length / 2);
    rlTexCoord2f(source.x / texWidth, source.y / texHeight);
    rlVertex3f(x - width / 2, y + height / 2, z - length / 2);

    rlEnd();

    rlSetTexture(0);
}