#pragma once
#include <raylib.h>
#include <vector>
#include <raymath.h>

// Simple structure for a single particle
struct Particle {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float size;
    float gravity;    // Gravity effect (positive = falls down, negative = floats up)
    float life;       // Current remaining life
    float startLife;  // Total lifetime (for fading/shrinking)
    bool active;
};

class ParticleSystem {
private:
    std::vector<Particle> particles;
    Texture2D particleTexture;

public:
    ParticleSystem();
    ~ParticleSystem();

    // Loads texture and sets correct filtering for pixel art look
    void init(); 

    // Main update loop (physics & aging)
    void update(float dt);

    // Draw all active particles (const so it can be called from const Scene methods)
    void draw(Camera camera) const;

    // Spawn methods
    // 'spread': how much random velocity to add
    void spawnExplosion(Vector3 center, int count, Color color, float size, float speed, float spread);
    
    // Spawn a directional burst (good for projectile impacts)
    void spawnDirectional(Vector3 center, Vector3 direction, int count, Color color, float speed, float spread);
    
    // Spawn a spiral pattern (good for summoning effects)
    void spawnSpiral(Vector3 center, float radius, int count, Color color, float height, float speed);
    
    // Spawn a ring that expands outward (good for healing/buffing)
    void spawnRing(Vector3 center, float radius, int count, Color color, float speed, bool upward);
};