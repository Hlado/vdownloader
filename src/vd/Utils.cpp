#include "Utils.h"

#include <thread>

namespace vd
{

unsigned int GetNumCores() noexcept
{
    //Can this actually change in runtime? Not likely, but...
    static auto cores = std::thread::hardware_concurrency();
    return cores == 0 ? 1 : cores;
}

} //namespace vd