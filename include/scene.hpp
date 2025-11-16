#include <vector>

// TODO maybe holds information for a single thing(pos, mesh...)
class Object
{
private:
public:
    bool collided(); // maybe change bool to "collision information" to better convey collision
};
class Scene
{
private:
    std::vector<Object> objects;
    Object floor;

public:
    void DrawScene();
    Scene() {};
    bool collided(); //TODO check collision for all objects
};