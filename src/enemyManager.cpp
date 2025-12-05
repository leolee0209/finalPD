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
        uc.scene->EmitDamageIndicator(*enemy, dResult.damage);
    }

    if (!enemy->damage(dResult))
    {
        TraceLog(LOG_ERROR, "enemy died\n");
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
