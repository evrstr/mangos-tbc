/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MANGOS_LOCALES_H
#define MANGOS_LOCALES_H

#include "Platform/Define.h"

#include <string>

/// 客户端支持的多语言区域枚举（与 DBC 本地化列索引对应）
enum LocaleConstant : uint8
{
    LOCALE_enUS = 0,                                        // also enGB | 美式英语（亦包含英式英语 enGB）
    LOCALE_koKR = 1,
    LOCALE_frFR = 2,
    LOCALE_deDE = 3,
    LOCALE_zhCN = 4,
    LOCALE_zhTW = 5,
    LOCALE_esES = 6,
    LOCALE_esMX = 7,
    LOCALE_ruRU = 8,
};

#define MAX_LOCALE 9
#define DEFAULT_LOCALE LOCALE_enUS

/// 根据语言代码字符串（如 "zhCN"、"enGB"）查找对应的 LocaleConstant 枚举
LocaleConstant GetLocaleByName(const std::string& name);

extern char const* localeNames[MAX_LOCALE];

/// 语言名称与 LocaleConstant 映射条目
struct LocaleNameStr
{
    char const* name;
    LocaleConstant locale;
};

// used for iterate all names including alternative | 包含别名（如 enGB）在内的完整语言代码映射表
extern LocaleNameStr const fullLocaleNameList[];

#endif
