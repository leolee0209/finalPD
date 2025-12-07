#include "enemyManager.hpp"
#include "me.hpp"
#include "scene.hpp"
void EnemyManager::RemoveEnemy(Enemy *e)
{
    int found = -1;
    for (int i = 0; i < this->enemies.size(); i++)
    {
        if (this->enemies[i] == e)
        {
            found = i;
            break;
        }
    }
    if (found != -1)
    {
        delete this->enemies[found];
        this->enemies.erase(this->enemies.begin() + found);
    }
}
EnemyManager::~EnemyManager()
{
    for (auto &e : this->enemies)
        delete e;
    this->enemies.clear();
}
void EnemyManager::addEnemy(Enemy *e)
{
    if (e)
        this->enemies.push_back(e);
}

void EnemyManager::update(UpdateContext &uc)
{
    // Iterate using an index over a snapshot of the original size
    // to avoid iterator/reference invalidation when enemies are added
    // (e.g., Summoner spawning Minions during UpdateBody).
    size_t originalCount = this->enemies.size();
    for (size_t i = 0; i < originalCount; ++i)
    {
        Enemy *e = this->enemies[i];
        if (e)
            e->UpdateBody(uc);
    }
}

std::vector<Object *> EnemyManager::getObjects() const
{
    std::vector<Object *> os;
    for (auto &e : this->enemies)
    {
        if (!e)
            continue;
        e->gatherObjects(os);
    }
    return os;
}

void EnemyManager::damage(Enemy *enemy, DamageResult &dResult, UpdateContext &uc)
{
    if (!enemy)
        return;

    if (uc.scene)
    {
        float effective = dResult.damage * (1.0f - enemy->damageResistance);
        Color color = (enemy->damageResistance > 0.0f) ? LIGHTGRAY : RED; // Metallic Gray feedback
        uc.scene->EmitDamageIndicator(*enemy, effective, color);
    }

    if (!enemy->damage(dResult))
    {        TraceLog(LOG_ERROR, "enemy died\n");
        
        // DEATH ANIMATION SEQUENCE
        if (uc.scene)
        {
            // Step 1: Hit Stop (0.05s pause)
            uc.scene->particles.triggerHitStop(0.05f);
            
            // Step 2 & 3: Flash white and spawn shards
            // Get enemy color from texture (use white if no texture)
            Color enemyColor = WHITE;
            if (enemy->obj().useTexture && enemy->obj().texture != nullptr && enemy->obj().texture->id != 0) {
                // Sample center of texture for enemy color
                Image img = LoadImageFromTexture(*enemy->obj().texture);
                if (img.data) {
                    Color* pixels = LoadImageColors(img);
                    int centerX = img.width / 2;
                    int centerY = img.height / 2;
                    if (centerX >= 0 && centerX < img.width && centerY >= 0 && centerY < img.height) {
                        enemyColor = pixels[centerY * img.width + centerX];
                    }
                    UnloadImageColors(pixels);
                    UnloadImage(img);
                }
            }
            
            // Spawn death shards at enemy position
            uc.scene->particles.spawnDeathShards(
                enemy->pos(), 
                enemyColor, 
                enemy->obj().size,
                dResult.directionHint,
                dResult.deathAnimationType
            );
        }
        
        // Call virtual OnDeath handler for special cleanup (e.g., Summoner minion cleanup)
        SummonerEnemy *summoner = dynamic_cast<SummonerEnemy*>(enemy);
        if (summoner)
        {
            summoner->OnDeath(uc);
        }
        this->RemoveEnemy(enemy);
    }
}

std::vector<Entity *> EnemyManager::getEntities(EntityCategory cat) const
{
    std::vector<Entity *> r;
    if (cat == ENTITY_ENEMY || cat == ENTITY_ALL)
    {
        for (auto &e : this->enemies)
            r.push_back(e);
    }
    return r;
}

void EnemyManager::clear()
{
    for (auto &e : this->enemies)
    {
        delete e;
    }
    this->enemies.clear();
}
