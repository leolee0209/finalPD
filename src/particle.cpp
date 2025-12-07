#include "particle.hpp"
#include <cstring>

ParticleSystem::ParticleSystem() {
    // Pre-allocate memory to avoid lag spikes during gameplay
    particles.reserve(2000);
    deathShards.reserve(200);  // Reserve space for death shards
}

ParticleSystem::~ParticleSystem() {
    if (IsWindowReady()) {
        UnloadTexture(particleTexture);
        // Clean up death shard meshes
        for (auto& shard : deathShards) {
            if (shard.meshInitialized && IsWindowReady()) {
                UnloadMesh(shard.mesh);
            }
        }
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
    // Update hit stop timer
    if (hitStopTimer > 0.0f) {
        hitStopTimer -= dt;
        if (hitStopTimer < 0.0f) hitStopTimer = 0.0f;
        // During hit stop, don't update particles/shards
        return;
    }
    
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
    
    // Update death shards
    for (auto& shard : deathShards) {
        if (!shard.active) continue;
        
        // Physics
        shard.position = Vector3Add(shard.position, Vector3Scale(shard.velocity, dt));
        shard.velocity.y -= 20.0f * dt;  // Gravity
        shard.velocity = Vector3Scale(shard.velocity, 0.98f);  // Air drag/friction
        
        // Rotation
        shard.rotationAngle += shard.rotationSpeed * dt;
        
        // Check floor collision (y = 0)
        if (shard.position.y <= 0.1f && shard.velocity.y < 0.0f) {
            shard.position.y = 0.0f;
            // Small bounce
            shard.velocity.y = -shard.velocity.y * 0.15f;
            // Reduce horizontal velocity (slide to stop)
            shard.velocity.x *= 0.7f;
            shard.velocity.z *= 0.7f;
            shard.rotationSpeed *= 0.5f;  // Slow down rotation
        }
        
        // Aging
        shard.life -= dt;
        
        // After 3 seconds, shrink to 0 over 0.5 seconds
        if (shard.life <= 0.5f) {
            float shrinkProgress = shard.life / 0.5f;  // 1.0 to 0.0
            Vector3 targetScale = Vector3Scale(shard.scale, shrinkProgress);
            shard.scale = targetScale;
        }
        
        if (shard.life <= 0.0f) {
            shard.active = false;
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

void ParticleSystem::drawDeathShards(Shader shader) const {
    for (const auto& shard : deathShards) {
        if (!shard.active || !shard.meshInitialized) continue;
        
        // Build transform matrix
        Matrix matScale = MatrixScale(shard.scale.x, shard.scale.y, shard.scale.z);
        Matrix matRotate = MatrixRotate(shard.rotationAxis, shard.rotationAngle * DEG2RAD);
        Matrix matTranslate = MatrixTranslate(shard.position.x, shard.position.y, shard.position.z);
        Matrix transform = MatrixMultiply(MatrixMultiply(matScale, matRotate), matTranslate);
        
        // Create material with the shard's color
        Material mat = LoadMaterialDefault();
        mat.maps[MATERIAL_MAP_DIFFUSE].color = shard.outerColor;
        mat.shader = shader;
        
        // Draw with shader for proper lighting
        DrawMesh(shard.mesh, mat, transform);
    }
}

void ParticleSystem::spawnDeathShards(Vector3 center, Color enemyColor, Vector3 size, Vector3 directionHint, int deathType) {
    // Determine shard count based on enemy size - use tweakable parameters
    float avgSize = (size.x + size.y + size.z) / 3.0f;
    int shardCount = this->shardCountNormal;
    if (avgSize < 1.0f) {
        shardCount = this->shardCountSmall;
    } else if (avgSize > 2.0f) {
        shardCount = this->shardCountLarge;
    }
    
    // Normalize direction hint
    Vector3 direction = directionHint;
    if (Vector3LengthSqr(direction) < 0.0001f) {
        direction = {0.0f, 0.0f, 1.0f};
    } else {
        direction = Vector3Normalize(direction);
    }
    
    // Spawn shards
    int spawned = 0;
    
    auto createShard = [&](DeathShard& shard, int index) {
        shard.active = true;
        shard.position = center;
        shard.life = 3.5f;  // 3 seconds + 0.5 for fade
        shard.startLife = shard.life;
        
        // Random shard size - use tweakable parameters
        float shardSize = this->shardBaseSize + (float)GetRandomValue(0, (int)(this->shardSizeVariation * 100)) / 100.0f;
        shardSize *= (avgSize / 1.5f);  // Scale with enemy size
        
        // Add shape variation - some shards are stretched/flattened
        float halfVar = this->shardScaleVariation * 0.5f;
        float scaleVariationX = (1.0f - halfVar) + (float)GetRandomValue(0, (int)(this->shardScaleVariation * 100)) / 100.0f;
        float scaleVariationY = (1.0f - halfVar) + (float)GetRandomValue(0, (int)(this->shardScaleVariation * 100)) / 100.0f;
        float scaleVariationZ = (1.0f - halfVar) + (float)GetRandomValue(0, (int)(this->shardScaleVariation * 100)) / 100.0f;
        
        shard.scale = {
            shardSize * scaleVariationX, 
            shardSize * scaleVariationY, 
            shardSize * scaleVariationZ
        };
        
        // Colors: outer = enemy color, inner = white
        shard.outerColor = enemyColor;
        shard.innerColor = WHITE;
        
        // Random rotation
        shard.rotationAxis = Vector3Normalize({
            (float)GetRandomValue(-100, 100) / 100.0f,
            (float)GetRandomValue(-100, 100) / 100.0f,
            (float)GetRandomValue(-100, 100) / 100.0f
        });
        shard.rotationAngle = (float)GetRandomValue(0, 360);
        shard.rotationSpeed = (float)GetRandomValue(180, 720);  // Fast tumbling
        
        // Velocity based on death type - use tweakable parameters
        Vector3 baseVel = {0, 0, 0};
        float speed = this->shardSpeed + (float)GetRandomValue(0, (int)(this->shardSpeedVariation * 10)) / 10.0f;
        
        switch (deathType) {
            case 0: {  // Melee: shards fly away from player
                float angle = (float)GetRandomValue(-45, 45) * DEG2RAD;
                Vector3 sideways = Vector3Normalize(Vector3CrossProduct(direction, {0, 1, 0}));
                Vector3 forward = Vector3RotateByAxisAngle(direction, {0, 1, 0}, angle);
                baseVel = Vector3Scale(forward, speed);
                baseVel.y = 3.0f + (float)GetRandomValue(0, 30) / 10.0f;  // Upward pop
                break;
            }
            case 1: {  // Projectile: cone behind enemy
                float angle = (float)GetRandomValue(-60, 60) * DEG2RAD;
                Vector3 backward = Vector3Scale(direction, -1.0f);
                Vector3 sideways = Vector3Normalize(Vector3CrossProduct(backward, {0, 1, 0}));
                Vector3 spread = Vector3RotateByAxisAngle(backward, {0, 1, 0}, angle);
                baseVel = Vector3Scale(spread, speed);
                baseVel.y = 2.0f + (float)GetRandomValue(0, 25) / 10.0f;
                break;
            }
            case 2:  // Magic/DOT: radial explosion
            default: {
                float angle1 = (float)GetRandomValue(0, 360) * DEG2RAD;
                float angle2 = (float)GetRandomValue(-30, 60) * DEG2RAD;  // Upward bias
                baseVel.x = cosf(angle1) * cosf(angle2) * speed;
                baseVel.z = sinf(angle1) * cosf(angle2) * speed;
                baseVel.y = sinf(angle2) * speed + 2.0f;
                break;
            }
        }
        
        shard.velocity = baseVel;
        
        // Create SHARP, VARIED shard mesh
        if (!shard.meshInitialized) {
            // Choose from multiple sharp shard shapes for variety
            int shapeVariant = GetRandomValue(0, 2);  // 3 different shapes
            
            float vertices[15];  // 5 vertices * 3 components
            unsigned short indices[18];  // 6 triangles * 3 indices
            int vertexCount = 5;
            int triangleCount = 6;
            
            // Randomize shape parameters for each shard
            float sharpness = 0.8f + (float)GetRandomValue(0, 40) / 100.0f;  // 0.8-1.2 (how pointy)
            float baseWidth = 0.5f + (float)GetRandomValue(0, 30) / 100.0f;  // 0.5-0.8
            float baseDepth = 0.3f + (float)GetRandomValue(0, 40) / 100.0f;  // 0.3-0.7
            
            switch (shapeVariant) {
                case 0: {  // Sharp pyramid with asymmetric base
                    vertices[0] = -baseWidth; vertices[1] = 0.0f; vertices[2] = -baseDepth * 0.5f;  // v0
                    vertices[3] = baseWidth * 0.8f; vertices[4] = 0.0f; vertices[5] = -baseDepth * 0.3f;  // v1
                    vertices[6] = baseWidth * 0.5f; vertices[7] = 0.0f; vertices[8] = baseDepth;  // v2
                    vertices[9] = -baseWidth * 0.6f; vertices[10] = 0.0f; vertices[11] = baseDepth * 0.8f;  // v3
                    vertices[12] = 0.0f; vertices[13] = sharpness * 1.2f; vertices[14] = 0.0f;  // v4 (apex)
                    
                    // Base quad (2 triangles) + 4 side triangles
                    indices[0] = 0; indices[1] = 1; indices[2] = 2;
                    indices[3] = 0; indices[4] = 2; indices[5] = 3;
                    indices[6] = 0; indices[7] = 4; indices[8] = 1;
                    indices[9] = 1; indices[10] = 4; indices[11] = 2;
                    indices[12] = 2; indices[13] = 4; indices[14] = 3;
                    indices[15] = 3; indices[16] = 4; indices[17] = 0;
                    break;
                }
                case 1: {  // Thin blade shard (elongated)
                    vertices[0] = -baseWidth * 0.3f; vertices[1] = 0.0f; vertices[2] = -baseDepth * 1.5f;  // v0
                    vertices[3] = baseWidth * 0.3f; vertices[4] = 0.0f; vertices[5] = -baseDepth * 1.5f;  // v1
                    vertices[6] = baseWidth * 0.2f; vertices[7] = 0.0f; vertices[8] = baseDepth * 1.2f;  // v2
                    vertices[9] = -baseWidth * 0.2f; vertices[10] = 0.0f; vertices[11] = baseDepth * 1.2f;  // v3
                    vertices[12] = 0.0f; vertices[13] = sharpness * 0.9f; vertices[14] = 0.0f;  // v4 (apex)
                    
                    indices[0] = 0; indices[1] = 1; indices[2] = 2;
                    indices[3] = 0; indices[4] = 2; indices[5] = 3;
                    indices[6] = 0; indices[7] = 4; indices[8] = 1;
                    indices[9] = 1; indices[10] = 4; indices[11] = 2;
                    indices[12] = 2; indices[13] = 4; indices[14] = 3;
                    indices[15] = 3; indices[16] = 4; indices[17] = 0;
                    break;
                }
                case 2:  // Jagged crystal (very sharp and angular)
                default: {
                    vertices[0] = -baseWidth * 0.7f; vertices[1] = 0.0f; vertices[2] = -baseDepth * 0.4f;  // v0
                    vertices[3] = baseWidth; vertices[4] = 0.0f; vertices[5] = -baseDepth * 0.6f;  // v1
                    vertices[6] = baseWidth * 0.4f; vertices[7] = 0.0f; vertices[8] = baseDepth * 1.3f;  // v2
                    vertices[9] = -baseWidth * 0.8f; vertices[10] = 0.0f; vertices[11] = baseDepth * 0.9f;  // v3
                    vertices[12] = 0.0f; vertices[13] = sharpness * 1.5f; vertices[14] = 0.1f;  // v4 (apex - offset)
                    
                    indices[0] = 0; indices[1] = 1; indices[2] = 2;
                    indices[3] = 0; indices[4] = 2; indices[5] = 3;
                    indices[6] = 0; indices[7] = 4; indices[8] = 1;
                    indices[9] = 1; indices[10] = 4; indices[11] = 2;
                    indices[12] = 2; indices[13] = 4; indices[14] = 3;
                    indices[15] = 3; indices[16] = 4; indices[17] = 0;
                    break;
                }
            }
            
            Mesh mesh = {0};
            mesh.triangleCount = triangleCount;
            mesh.vertexCount = vertexCount;
            
            mesh.vertices = (float *)MemAlloc(vertexCount * 3 * sizeof(float));
            memcpy(mesh.vertices, vertices, vertexCount * 3 * sizeof(float));
            
            mesh.indices = (unsigned short *)MemAlloc(triangleCount * 3 * sizeof(unsigned short));
            memcpy(mesh.indices, indices, triangleCount * 3 * sizeof(unsigned short));
            
            // Generate normals (simple upward bias for now)
            mesh.normals = (float *)MemAlloc(vertexCount * 3 * sizeof(float));
            for (int i = 0; i < vertexCount; i++) {
                mesh.normals[i*3 + 0] = 0.0f;
                mesh.normals[i*3 + 1] = 1.0f;
                mesh.normals[i*3 + 2] = 0.0f;
            }
            
            UploadMesh(&mesh, false);
            shard.mesh = mesh;
            shard.meshInitialized = true;
        }
    };
    
    // Try to reuse inactive shards
    for (auto& shard : deathShards) {
        if (!shard.active) {
            createShard(shard, spawned);
            spawned++;
            if (spawned >= shardCount) return;
        }
    }
    
    // Create new shards if needed
    for (int i = spawned; i < shardCount; i++) {
        DeathShard shard;
        createShard(shard, i);
        deathShards.push_back(shard);
    }
    
    // Spawn dust puff at death location
    this->spawnExplosion(center, 8, ColorAlpha(WHITE, 150), 0.2f, 1.5f, 0.8f);
}