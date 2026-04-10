#ifndef GAME_TMX_LOADER_HPP
#define GAME_TMX_LOADER_HPP

#include "game/LevelData.hpp"

namespace Game {
LevelConfig LoadLevelFromTmx(const std::string &relativeMapPath);
} // namespace Game

#endif
