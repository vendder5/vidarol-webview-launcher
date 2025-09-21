#pragma once

#include <windows.h>
#include <string>
#include "obfuscate.h"

namespace Injector
{
    bool injectSamp(const std::string& name, const std::string& ip, int port);
}