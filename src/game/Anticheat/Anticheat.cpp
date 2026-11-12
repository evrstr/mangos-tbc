#ifdef USE_ANTICHEAT
#error "This file should not be included when the real anticheat module is present"
#endif

#include "Anticheat.hpp"

// 未启用反作弊模块（USE_ANTICHEAT 未定义）时返回空实现单例接口
AnticheatLibInterface* GetAnticheatLib()
{
    static NullAnticheatLib l;
    return &l;
}