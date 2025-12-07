#include "wallTile.hpp"

WallTile::WallTile(TileModelManager* mgr, TileType tType, Vector3 pos, Quaternion rot, float sFactor)
    : TileObject(mgr, tType, pos, rot, sFactor)
{
}
