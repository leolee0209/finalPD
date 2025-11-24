#include "enemyManager.hpp"
#include "me.hpp"
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
    for (auto &e : this->enemies)
        e->UpdateBody(uc);
}

std::vector<Object *> EnemyManager::getObjects() const
{
    std::vector<Object *> os;
    for (auto &e : this->enemies)
        os.push_back(&e->obj());
    return os;
}

void EnemyManager::damage(Enemy *enemy, DamageResult &dResult)
{
    if (!enemy->damage(dResult))
        this->RemoveEnemy(enemy);
}

std::vector<Entity *> EnemyManager::getEntities()
{
    std::vector<Entity *> r;
    for (auto &e : this->enemies)
        r.push_back(e);
    return r;
}
