#include "particle.hpp"

ParticleSystem::ParticleSystem() {
    // Pre-allocate memory to avoid lag spikes during gameplay
    particles.reserve(2000);
}

ParticleSystem::~ParticleSystem() {
    if (IsWindowReady()) {
        UnloadTexture(particleTexture);
    }
}

void ParticleSystem::init() {
    // 1. Generate a simple 16x16 white pixel texture (Minecraft style)
    // If you downloaded a sprite sheet, load it here instead: LoadTexture("resources/particles.png");
    Image img = GenImageColor(16, 16, WHITE); 
    particleTexture = LoadTextureFromImage(img);
    UnloadImage(img);

    // 2. CRITICAL: Set Point Filtering
    // This disables linear interpolation, keeping the "blocky" look when scaled up
    SetTextureFilter(particleTexture, TEXTURE_FILTER_POINT);
}

void ParticleSystem::update(float dt) {
    for (auto& p : particles) {
        if (!p.active) continue;

        // 1. Movement
        p.position = Vector3Add(p.position, Vector3Scale(p.velocity, dt));
        
        // 2. Gravity (Minecraft particles usually fall slowly or float)
        p.velocity.y -= p.gravity * dt; 

        // 3. Drag/Friction (Slow down over time)
        p.velocity = Vector3Scale(p.velocity, 0.95f);

        // 4. Aging
        p.life -= dt;
        if (p.life <= 0.0f) {
            p.active = false;
        }
    }
}

void ParticleSystem::draw(Camera camera) const
{
    // 1. Use ALPHA blending (Standard transparency, not glowing)
    BeginBlendMode(BLEND_ALPHA);
    
    // 2. We DO NOT disable depth writing. 
    // This allows particles to hide behind walls correctly.
    
    for (const auto& p : particles) {
        if (p.active) {
            // Optional: Fade out near end of life
            Color drawColor = p.color;
            if (p.life < 0.5f) {
                drawColor.a = (unsigned char)((p.life / 0.5f) * 255);
            }
            
            DrawBillboard(camera, particleTexture, p.position, p.size, drawColor);
        }
    }

    EndBlendMode();
}

void ParticleSystem::spawnExplosion(Vector3 center, int count, Color color, float size, float speed, float spread) {
    int spawned = 0;
    
    // Helper lambda to init a particle
    auto initParticle = [&](Particle& p) {
        p.active = true;
        p.position = center;
        
        // Random cube-like velocity
        Vector3 randomDir = { 
            (float)GetRandomValue(-100, 100) / 100.0f, 
            (float)GetRandomValue(-100, 100) / 100.0f, 
            (float)GetRandomValue(-100, 100) / 100.0f 
        };
        p.velocity = Vector3Scale(Vector3Normalize(randomDir), speed);
        
        p.color = color;
        p.size = size * this->globalSizeMultiplier;
        // Apply global intensity to alpha (preserve existing alpha if set)
        int alpha = (int)(p.color.a * this->globalIntensityMultiplier);
        if (alpha > 255) alpha = 255;
        p.color.a = (unsigned char)alpha;
        p.gravity = 2.0f; // Gravity makes them fall like debris
        p.startLife = 1.0f + (float)GetRandomValue(0, 50)/100.0f; // Random life 1.0 - 1.5s
        p.life = p.startLife;
    };

    // 1. Reuse inactive particles
    for (auto& p : particles) {
        if (!p.active) {
            initParticle(p);
            spawned++;
            if (spawned >= count) return;
        }
    }

    // 2. Create new if needed
    for (int i = spawned; i < count; i++) {
        Particle p;
        initParticle(p);
        particles.push_back(p);
    }
}

void ParticleSystem::spawnDirectional(Vector3 center, Vector3 direction, int count, Color color, float speed, float spread) {
    int spawned = 0;
    
    // Normalize direction
    Vector3 baseDir = Vector3Normalize(direction);
    
    auto initParticle = [&](Particle& p) {
        p.active = true;
        p.position = center;
        
        // Add random spread around base direction
        Vector3 randomOffset = { 
            (float)GetRandomValue(-100, 100) / 100.0f * spread, 
            (float)GetRandomValue(-100, 100) / 100.0f * spread, 
            (float)GetRandomValue(-100, 100) / 100.0f * spread 
        };
        Vector3 finalDir = Vector3Add(baseDir, randomOffset);
        p.velocity = Vector3Scale(Vector3Normalize(finalDir), speed);
        
        p.color = color;
        p.size = (0.15f + (float)GetRandomValue(0, 10) / 100.0f) * this->globalSizeMultiplier; // 0.15-0.25
        int alpha2 = (int)(p.color.a * this->globalIntensityMultiplier);
        if (alpha2 > 255) alpha2 = 255;
        p.color.a = (unsigned char)alpha2;
        p.gravity = 1.0f; // Light gravity
        p.startLife = 0.8f + (float)GetRandomValue(0, 40)/100.0f; // 0.8-1.2s
        p.life = p.startLife;
    };

    // Reuse inactive particles
    for (auto& p : particles) {
        if (!p.active) {
            initParticle(p);
            spawned++;
            if (spawned >= count) return;
        }
    }

    // Create new if needed
    for (int i = spawned; i < count; i++) {
        Particle p;
        initParticle(p);
        particles.push_back(p);
    }
}

void ParticleSystem::spawnSpiral(Vector3 center, float radius, int count, Color color, float height, float speed) {
    int spawned = 0;
    
    auto initParticle = [&](Particle& p, int index) {
        p.active = true;
        
        // Calculate spiral position
        float angle = (index * PI * 2.0f) / count;
        float spiralRadius = radius * ((float)index / count);
        
        p.position.x = center.x + cosf(angle) * spiralRadius;
        p.position.y = center.y + ((float)index / count) * height;
        p.position.z = center.z + sinf(angle) * spiralRadius;
        
        // Velocity spirals outward and upward
        p.velocity = {
            cosf(angle) * speed * 0.5f,
            speed * 0.3f,
            sinf(angle) * speed * 0.5f
        };
        
        p.color = color;
        p.size = 0.2f * this->globalSizeMultiplier;
        int alpha3 = (int)(p.color.a * this->globalIntensityMultiplier);
        if (alpha3 > 255) alpha3 = 255;
        p.color.a = (unsigned char)alpha3;
        p.gravity = -0.5f; // Float upward
        p.startLife = 1.5f;
        p.life = p.startLife;
    };

    int particleIndex = 0;
    // Reuse inactive particles
    for (auto& p : particles) {
        if (!p.active) {
            initParticle(p, particleIndex++);
            spawned++;
            if (spawned >= count) return;
        }
    }

    // Create new if needed
    for (int i = spawned; i < count; i++) {
        Particle p;
        initParticle(p, particleIndex++);
        particles.push_back(p);
    }
}

void ParticleSystem::spawnRing(Vector3 center, float radius, int count, Color color, float speed, bool upward) {
    int spawned = 0;
    
    auto initParticle = [&](Particle& p, int index) {
        p.active = true;
        
        // Calculate ring position
        float angle = (index * PI * 2.0f) / count;
        
        p.position.x = center.x + cosf(angle) * radius * 0.3f;
        p.position.y = center.y;
        p.position.z = center.z + sinf(angle) * radius * 0.3f;
        
        // Velocity shoots outward
        p.velocity = {
            cosf(angle) * speed,
            upward ? speed * 0.5f : 0.0f,
            sinf(angle) * speed
        };
        
        p.color = color;
        p.size = 0.25f * this->globalSizeMultiplier;
        int alpha4 = (int)(p.color.a * this->globalIntensityMultiplier);
        if (alpha4 > 255) alpha4 = 255;
        p.color.a = (unsigned char)alpha4;
        p.gravity = 0.5f;
        p.startLife = 1.0f;
        p.life = p.startLife;
    };

    int particleIndex = 0;
    // Reuse inactive particles
    for (auto& p : particles) {
        if (!p.active) {
            initParticle(p, particleIndex++);
            spawned++;
            if (spawned >= count) return;
        }
    }

    // Create new if needed
    for (int i = spawned; i < count; i++) {
        Particle p;
        initParticle(p, particleIndex++);
        particles.push_back(p);
    }
}