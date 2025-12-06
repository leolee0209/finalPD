#pragma once
#include "scene.hpp"

class WorldGenerator {
public:
    static void Generate(Scene* scene);

private:
    static void GenerateDiscardMaze(Scene* scene, Vector3 startOffset);
    static void GenerateGreatWall(Scene* scene, Vector3 startOffset);
    static void GenerateArena(Scene* scene, Vector3 startOffset);
};
