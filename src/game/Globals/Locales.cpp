#include "Locales.h"

/// 按 LocaleConstant 索引的标准语言代码名称表
char const* localeNames[MAX_LOCALE] =
{
    "enUS",                                                 // also enGB | 亦包含 enGB
    "koKR",
    "frFR",
    "deDE",
    "zhCN",
    "zhTW",
    "esES",
    "esMX",
    "ruRU",
};

// used for search by name or iterate all names | 包含 enGB 别名在内的完整语言名称查找表
LocaleNameStr const fullLocaleNameList[] =
{
    { "enUS",   LOCALE_enUS },
    { "enGB",   LOCALE_enUS },
    { "koKR",   LOCALE_koKR },
    { "frFR",   LOCALE_frFR },
    { "deDE",   LOCALE_deDE },
    { "zhCN",   LOCALE_zhCN },
    { "zhTW",   LOCALE_zhTW },
    { "esES",   LOCALE_esES },
    { "esMX",   LOCALE_esMX },
    { "ruRU",   LOCALE_ruRU },
    { nullptr,  LOCALE_enUS }
};

/// 将语言代码字符串解析为 LocaleConstant，未匹配时回退至 LOCALE_enUS
LocaleConstant GetLocaleByName(const std::string& name)
{
    for (LocaleNameStr const* itr = &fullLocaleNameList[0]; itr->name; ++itr)
        if (name == itr->name)
            return itr->locale;

    return LOCALE_enUS;                                     // including enGB case | 默认回退英文区域
}
