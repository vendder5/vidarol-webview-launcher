#pragma once

#include <string>

namespace Config
{
    void savePlayerName(const std::string& name);
    std::string loadPlayerName();
}
