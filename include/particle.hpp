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

// Death shard structure for enemy death animations
struct DeathShard {
    Vector3 position;
    Vector3 velocity;
    Vector3 rotationAxis;
    float rotationAngle;
    float rotationSpeed;
    Vector3 scale;
    Color outerColor;  // Colored face (enemy texture color)
    Color innerColor;  // White inner material
    float life;
    float startLife;
    bool active;
    Mesh mesh;
    bool meshInitialized;
    
    DeathShard() : position{0}, velocity{0}, rotationAxis{0,1,0}, 
                   rotationAngle(0), rotationSpeed(0), scale{1,1,1},
                   outerColor(WHITE), innerColor(WHITE), 
                   life(0), startLife(0), active(false), 
                   mesh{0}, meshInitialized(false) {}
};

class ParticleSystem {
private:
    std::vector<Particle> particles;
    std::vector<DeathShard> deathShards;
    Texture2D particleTexture;
    float hitStopTimer = 0.0f;  // Timer for hit stop effect

public:
    ParticleSystem();
    ~ParticleSystem();

    // Loads texture and sets correct filtering for pixel art look
    void init(); 

    // Main update loop (physics & aging)
    void update(float dt);

    // Draw all active particles (const so it can be called from const Scene methods)
    void draw(Camera camera) const;
    
    // Draw death shards with 3D rendering (needs shader for lighting)
    void drawDeathShards(Shader shader) const;

    // Spawn methods
    // 'spread': how much random velocity to add
    void spawnExplosion(Vector3 center, int count, Color color, float size, float speed, float spread);
    
    // Spawn a directional burst (good for projectile impacts)
    void spawnDirectional(Vector3 center, Vector3 direction, int count, Color color, float speed, float spread);
    
    // Spawn a spiral pattern (good for summoning effects)
    void spawnSpiral(Vector3 center, float radius, int count, Color color, float height, float speed);
    
    // Spawn a ring that expands outward (good for healing/buffing)
    void spawnRing(Vector3 center, float radius, int count, Color color, float speed, bool upward);
    
    // Spawn death shards for enemy death animation
    // deathType: 0=melee, 1=projectile, 2=magic/dot
    void spawnDeathShards(Vector3 center, Color enemyColor, Vector3 size, Vector3 directionHint, int deathType);

    // Global multipliers to tweak visuals at runtime
    float globalSizeMultiplier = 1.0f;    // Multiply particle sizes
    float globalIntensityMultiplier = 1.0f; // Multiply particle alpha/intensity
    
    // Death shard tweakable parameters
    float shardBaseSize = 0.4f;           // Base shard size (0.4-1.0 range)
    float shardSizeVariation = 0.6f;      // Size variation amount (0.0-1.0)
    float shardScaleVariation = 0.6f;     // Per-axis scale variation (0.0-1.0)
    int shardCountSmall = 12;             // Shard count for small enemies
    int shardCountNormal = 18;            // Shard count for normal enemies
    int shardCountLarge = 25;             // Shard count for large enemies
    float shardSpeed = 8.0f;              // Base shard velocity
    float shardSpeedVariation = 4.0f;     // Speed variation range
    
    // Check if hit stop is active
    bool isHitStopActive() const { return hitStopTimer > 0.0f; }
    // Trigger hit stop
    void triggerHitStop(float duration) { hitStopTimer = duration; }
};