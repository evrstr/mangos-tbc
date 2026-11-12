
#ifndef EXTRACTOR_DEFINES_H
#define EXTRACTOR_DEFINES_H

// 网格地图文件头，描述区域、高度、液体及地形孔洞数据块在文件中的偏移与大小
struct GridMapFileHeader
{
    uint32 mapMagic;
    uint32 versionMagic;
    uint32 areaMapOffset;
    uint32 areaMapSize;
    uint32 heightMapOffset;
    uint32 heightMapSize;
    uint32 liquidMapOffset;
    uint32 liquidMapSize;
    uint32 holesOffset;
    uint32 holesSize;
};

#define MAP_AREA_NO_AREA      0x0001

// 区域信息块头，存储网格全局区域标志与默认区域 ID
struct GridMapAreaHeader
{
    uint32 fourcc;
    uint16 flags;
    uint16 gridArea;
};

#define MAP_HEIGHT_NO_HEIGHT  0x0001
#define MAP_HEIGHT_AS_INT16   0x0002
#define MAP_HEIGHT_AS_INT8    0x0004

// 高度图信息块头，存储网格基准高度与最大高度
struct GridMapHeightHeader
{
    uint32 fourcc;
    uint32 flags;
    float gridHeight;
    float gridMaxHeight;
};

#define MAP_LIQUID_NO_TYPE    0x01
#define MAP_LIQUID_NO_HEIGHT  0x02

// 液体信息块头，描述液体类型、覆盖区域范围及基础液面高度
struct GridMapLiquidHeader
{
    uint32 fourcc;
    uint8 flags;
    uint8 liquidFlags;
    uint16 liquidType;
    uint8 offsetX;
    uint8 offsetY;
    uint8 width;
    uint8 height;
    float liquidLevel;
};

// 网格地图液体状态枚举（无水、水面之上、水上行走、水中、水下）
enum GridMapLiquidStatus
{
    LIQUID_MAP_NO_WATER = 0x00000000,
    LIQUID_MAP_ABOVE_WATER = 0x00000001,
    LIQUID_MAP_WATER_WALK = 0x00000002,
    LIQUID_MAP_IN_WATER = 0x00000004,
    LIQUID_MAP_UNDER_WATER = 0x00000008
};

// defined in DBC and left shifted for flag usage
// 在 DBC 中定义，并左移用作标志位
#define MAP_LIQUID_TYPE_NO_WATER    0x00
#define MAP_LIQUID_TYPE_MAGMA       0x01
#define MAP_LIQUID_TYPE_OCEAN       0x02
#define MAP_LIQUID_TYPE_SLIME       0x04
#define MAP_LIQUID_TYPE_WATER       0x08

#define MAP_ALL_LIQUIDS   (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_MAGMA | MAP_LIQUID_TYPE_OCEAN | MAP_LIQUID_TYPE_SLIME)

#define MAP_LIQUID_TYPE_DEEP_WATER  0x10
#define MAP_LIQUID_TYPE_WMO_WATER   0x20

// 液体查询结果数据，包含液体类型标志、DBC 条目、液面高度与水底深度
struct GridMapLiquidData
{
    uint32 type_flags;
    uint32 entry;
    float level;
    float depth_level;
};
#endif
