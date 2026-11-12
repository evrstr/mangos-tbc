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

#ifndef _UTIL_H
#define _UTIL_H

#include "Common.h"

#include <string>
#include <vector>
#include <random>

typedef std::vector<std::string> Tokens;

/// 按分隔符集合拆分字符串
Tokens StrSplit(const std::string& src, const std::string& sep);
/// 从拆分数组中读取指定下标的无符号 32 位整数
uint32 GetUInt32ValueFromArray(Tokens const& data, uint16 index);
/// 从拆分数组中读取指定下标的浮点数（按位解释 uint32）
float GetFloatValueFromArray(Tokens const& data, uint16 index);

/// 折叠并替换行内不可见控制字符（空白、制表符、响铃、换行）为单个空格
void stripLineInvisibleChars(std::string& str);

/// 计算指定本地小时（hour）对应的时间戳
time_t GetLocalHourTimestamp(time_t time, uint8 hour, bool onlyAfterTime = true);
/// 将秒数格式化为可读时间长度字符串（支持简写如 1d2h 与仅小时模式）
std::string secsToTimeString(time_t timeInSecs, bool shortText = false, bool hoursOnly = false);
/// 解析诸如 "1d2h30m" 格式的时间字符串为总秒数
uint32 TimeStringToSecs(const std::string& timestring);
/// 将时间戳格式化为 YYYY-MM-DD_HH-MM-SS 字符串
std::string TimeToTimestampStr(time_t t);
/// 将 DBC/DB 打包时间位域还原为 time_t
time_t timeBitFieldsToSecs(uint32 packedDate);

/// 将 time_t 打包为客户端任务/邮件等使用的 32 位时间位域格式
inline uint32 secsToTimeBitFields(time_t secs)
{
    tm* lt = localtime(&secs);
    return (lt->tm_year - 100) << 24 | lt->tm_mon  << 20 | (lt->tm_mday - 1) << 14 | lt->tm_wday << 11 | lt->tm_hour << 6 | lt->tm_min;
}

/// 获取当前线程局部的 Mersenne Twister 随机数引擎实例
std::mt19937* GetRandomGenerator();

/* Return a random number in the range min..max; (max-min) must be smaller than 32768. | 返回 [min, max] 范围内的随机整数；(max-min) 应小于 32768 */
int32 irand(int32 min, int32 max);

/* Return a random number in the range min..max (inclusive). For reliable results, the difference
* between max and min should be less than RAND32_MAX. | 返回 [min, max] 闭区间内的随机无符号整数 */
uint32 urand(uint32 min, uint32 max);

/* Return a random number in the range min..max (inclusive). | 返回 [min, max] 范围内的随机浮点数 */
float frand(float min, float max);

/* Return a random number in the range RAND32_MIN .. RAND32_MAX. | 返回全范围随机有符号 32 位整数 */
int32 irand();

/* Return a random number in the range 0 .. RAND32_MAX. | 返回全范围随机无符号 32 位整数 */
uint32 urand();

/* Return a random double from 0.0 to 1.0 (exclusive). Floats support only 7 valid decimal digits.
 * A double supports up to 15 valid decimal digits and is used internally (RAND32_MAX has 10 digits).
 * With an FPU, there is usually no difference in performance between float and double. | 返回 [0.0, 1.0) 区间的双精度随机浮点数 */
double rand_norm(void);

float rand_norm_f(void);

/* Return a random double from 0.0 to 99.9999999999999. Floats support only 7 valid decimal digits.
 * A double supports up to 15 valid decimal digits and is used internaly (RAND32_MAX has 10 digits).
 * With an FPU, there is usually no difference in performance between float and double. | 返回 [0.0, 100.0) 区间的双精度百分比随机数 */
double rand_chance(void);

float rand_chance_f(void);

/* Return true if a random roll fits in the specified chance (range 0-100). | 按浮点概率（0-100）判定随机投骰是否成功 */
inline bool roll_chance_f(float chance)
{
    return chance > rand_chance();
}

/* Return true if a random roll fits in the specified chance (range 0-100). | 按整型概率（0-100）判定随机投骰是否成功 */
inline bool roll_chance_i(int chance)
{
    return chance > irand(0, 99);
}

/* Convert floating point chance to premultiplied integer chance (100.00 = 10000). | 将浮点概率放大 100 倍转为万分比整数（100.00% -> 10000） */
inline uint32 chance_u(float chance)
{
    return uint32(::roundf(std::max(0.0f, chance) * 100)); // Nearest 2 decimal places | 精确到小数点后两位
}

/* Perform a quick non-die combat roll with premultiplied integer chance | 使用预乘万分比概率执行快速单次战斗概率判定 */
inline bool roll_chance_combat(float chance)
{
    uint32 u = chance_u(chance);
    return (u && (u > urand(1, 10000)));
}

/* An abstract die for combat rolls with premultiplied integer chances | 用于战斗圆桌判定（Combat Roll Table）的抽象多面骰模板结构 */
template<class Side, Side Default, uint8 Sides>
struct Die
{
    // MSVC++13-friendly initialization, switch to {0} when we end support for it | 兼容旧版 MSVC 的数组清零初始化写法
    explicit Die() { for (uint8 i = 0; i < Sides; ++i) chance[i] = 0; }
    Side roll(uint32 random)
    {
        uint32 rolling = 0;
        for (uint8 side = 0; side < Sides; ++side)
        {
            if (chance[side])
            {
                rolling += chance[side];
                if (random <= rolling)
                    return Side(side);
            }
        }
        return Default;
    }
    void set(uint8 side, float chancef)
    {
        if (side < Sides)
            chance[side] = chance_u(chancef);
    }
    uint32 chance[Sides];
};

template<typename T, typename... Args>
T PickRandomValue(T first, Args ...rest)
{
    T array[sizeof...(rest) + 1] = { first, rest... };
    return array[urand(0, (sizeof...(rest)))];
}

inline void ApplyModUInt32Var(uint32& var, int32 val, bool apply)
{
    int32 cur = var;
    cur += (apply ? val : -val);
    if (cur < 0)
        cur = 0;
    var = cur;
}

inline void ApplyModFloatVar(float& var, float  val, bool apply)
{
    var += (apply ? val : -val);
    if (var < 0)
        var = 0;
}

inline void ApplyPercentModFloatVar(float& var, float val, bool apply)
{
    if (val == -100.0f)     // prevent set var to zero | 避免 -100% 导致后续逆运算除零
        val = -99.99f;
    var *= (apply ? (100.0f + val) / 100.0f : 100.0f / (100.0f + val));
}

/// 将 UTF-8 字符串转换为宽字符串（可选截断最大字符数 max_len）
bool Utf8toWStr(const std::string& utf8str, std::wstring& wstr, size_t max_len = 0);
// in wsize==max size of buffer, out wsize==real string size | 输入 wsize 为最大长度限制，输出实际转换长度

/// 将宽字符串转换为 UTF-8 字符串
bool WStrToUtf8(const std::wstring& wstr, std::string& utf8str);

/// 返回 UTF-8 字符个数，若序列非法则将字符串清空并返回 0
size_t utf8length(std::string& utf8str);                    // returns string's length in utf8 chars, sets string to "" on invalid utf8 sequence | 返回 UTF-8 字符数，遇非法序列置空
/// 按最大字节数截断 UTF-8 字符串且保证不在多字节字符中间切断
size_t utf8limit(std::string& utf8str, size_t bytes);       // returns string's new size in bytes, sets string to "" on invalid utf8 sequence | 按字节上限安全截断 UTF-8，遇非法序列置空
/// 按最大字符数截断 UTF-8 字符串
void utf8truncate(std::string& utf8str, size_t len);

inline bool isBasicLatinCharacter(wchar_t wchar)
{
    if (wchar >= L'a' && wchar <= L'z')                     // LATIN SMALL LETTER A - LATIN SMALL LETTER Z
        return true;
    if (wchar >= L'A' && wchar <= L'Z')                     // LATIN CAPITAL LETTER A - LATIN CAPITAL LETTER Z
        return true;
    return false;
}

inline bool isExtendedLatinCharacter(wchar_t wchar)
{
    if (isBasicLatinCharacter(wchar))
        return true;
    if (wchar >= 0x00C0 && wchar <= 0x00D6)                 // LATIN CAPITAL LETTER A WITH GRAVE - LATIN CAPITAL LETTER O WITH DIAERESIS
        return true;
    if (wchar >= 0x00D8 && wchar <= 0x00DF)                 // LATIN CAPITAL LETTER O WITH STROKE - LATIN CAPITAL LETTER THORN
        return true;
    if (wchar == 0x00DF)                                    // LATIN SMALL LETTER SHARP S
        return true;
    if (wchar >= 0x00E0 && wchar <= 0x00F6)                 // LATIN SMALL LETTER A WITH GRAVE - LATIN SMALL LETTER O WITH DIAERESIS
        return true;
    if (wchar >= 0x00F8 && wchar <= 0x00FE)                 // LATIN SMALL LETTER O WITH STROKE - LATIN SMALL LETTER THORN
        return true;
    if (wchar >= 0x0100 && wchar <= 0x012F)                 // LATIN CAPITAL LETTER A WITH MACRON - LATIN SMALL LETTER I WITH OGONEK
        return true;
    if (wchar == 0x1E9E)                                    // LATIN CAPITAL LETTER SHARP S
        return true;
    return false;
}

inline bool isCyrillicCharacter(wchar_t wchar)
{
    if (wchar >= 0x0410 && wchar <= 0x044F)                 // CYRILLIC CAPITAL LETTER A - CYRILLIC SMALL LETTER YA
        return true;
    if (wchar == 0x0401 || wchar == 0x0451)                 // CYRILLIC CAPITAL LETTER IO, CYRILLIC SMALL LETTER IO
        return true;
    return false;
}

inline bool isEastAsianCharacter(wchar_t wchar)
{
    if (wchar >= 0x1100 && wchar <= 0x11F9)                 // Hangul Jamo
        return true;
    if (wchar >= 0x3041 && wchar <= 0x30FF)                 // Hiragana + Katakana
        return true;
    if (wchar >= 0x3131 && wchar <= 0x318E)                 // Hangul Compatibility Jamo
        return true;
    if (wchar >= 0x31F0 && wchar <= 0x31FF)                 // Katakana Phonetic Ext.
        return true;
    if (wchar >= 0x3400 && wchar <= 0x4DB5)                 // CJK Ideographs Ext. A
        return true;
    if (wchar >= 0x4E00 && wchar <= 0x9FC3)                 // Unified CJK Ideographs
        return true;
    if (wchar >= 0xAC00 && wchar <= 0xD7A3)                 // Hangul Syllables
        return true;
    if (wchar >= 0xFF01 && wchar <= 0xFFEE)                 // Halfwidth forms
        return true;
    return false;
}

inline bool isWhiteSpace(char c)
{
    return ::isspace(int(c)) != 0;
}

inline bool isNumeric(wchar_t wchar)
{
    return (wchar >= L'0' && wchar <= L'9');
}

inline bool isNumeric(char c)
{
    return (c >= '0' && c <= '9');
}

inline bool isNumericOrSpace(wchar_t wchar)
{
    return isNumeric(wchar) || wchar == L' ';
}

inline bool isNumeric(char const* str)
{
    for (char const* c = str; *c; ++c)
        if (!isNumeric(*c))
            return false;

    return true;
}

inline bool isNumeric(std::string const& str)
{
    for (char itr : str)
        if (!isNumeric(itr))
            return false;

    return true;
}

inline bool isNumeric(std::wstring const& str)
{
    for (wchar_t itr : str)
        if (!isNumeric(itr))
            return false;

    return true;
}

inline bool isBasicLatinString(const std::wstring& wstr, bool numericOrSpace)
{
    for (wchar_t i : wstr)
        if (!isBasicLatinCharacter(i) && (!numericOrSpace || !isNumericOrSpace(i)))
            return false;
    return true;
}

inline bool isExtendedLatinString(const std::wstring& wstr, bool numericOrSpace)
{
    for (wchar_t i : wstr)
        if (!isExtendedLatinCharacter(i) && (!numericOrSpace || !isNumericOrSpace(i)))
            return false;
    return true;
}

inline bool isCyrillicString(const std::wstring& wstr, bool numericOrSpace)
{
    for (wchar_t i : wstr)
        if (!isCyrillicCharacter(i) && (!numericOrSpace || !isNumericOrSpace(i)))
            return false;
    return true;
}

inline bool isEastAsianString(const std::wstring& wstr, bool numericOrSpace)
{
    for (wchar_t i : wstr)
        if (!isEastAsianCharacter(i) && (!numericOrSpace || !isNumericOrSpace(i)))
            return false;
    return true;
}

inline void strToUpper(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), toupper);
}

inline void strToLower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), tolower);
}

inline wchar_t wcharToUpper(wchar_t wchar)
{
    if (wchar >= L'a' && wchar <= L'z')                     // LATIN SMALL LETTER A - LATIN SMALL LETTER Z
        return wchar_t(uint16(wchar) - 0x0020);
    if (wchar == 0x00DF)                                    // LATIN SMALL LETTER SHARP S
        return wchar_t(0x1E9E);
    if (wchar >= 0x00E0 && wchar <= 0x00F6)                 // LATIN SMALL LETTER A WITH GRAVE - LATIN SMALL LETTER O WITH DIAERESIS
        return wchar_t(uint16(wchar) - 0x0020);
    if (wchar >= 0x00F8 && wchar <= 0x00FE)                 // LATIN SMALL LETTER O WITH STROKE - LATIN SMALL LETTER THORN
        return wchar_t(uint16(wchar) - 0x0020);
    if (wchar >= 0x0101 && wchar <= 0x012F)                 // LATIN SMALL LETTER A WITH MACRON - LATIN SMALL LETTER I WITH OGONEK (only %2=1)
    {
        if (wchar % 2 == 1)
            return wchar_t(uint16(wchar) - 0x0001);
    }
    if (wchar >= 0x0430 && wchar <= 0x044F)                 // CYRILLIC SMALL LETTER A - CYRILLIC SMALL LETTER YA
        return wchar_t(uint16(wchar) - 0x0020);
    if (wchar == 0x0451)                                    // CYRILLIC SMALL LETTER IO
        return wchar_t(0x0401);

    return wchar;
}

inline wchar_t wcharToUpperOnlyLatin(wchar_t wchar)
{
    return isBasicLatinCharacter(wchar) ? wcharToUpper(wchar) : wchar;
}

inline wchar_t wcharToLower(wchar_t wchar)
{
    if (wchar >= L'A' && wchar <= L'Z')                     // LATIN CAPITAL LETTER A - LATIN CAPITAL LETTER Z
        return wchar_t(uint16(wchar) + 0x0020);
    if (wchar >= 0x00C0 && wchar <= 0x00D6)                 // LATIN CAPITAL LETTER A WITH GRAVE - LATIN CAPITAL LETTER O WITH DIAERESIS
        return wchar_t(uint16(wchar) + 0x0020);
    if (wchar >= 0x00D8 && wchar <= 0x00DE)                 // LATIN CAPITAL LETTER O WITH STROKE - LATIN CAPITAL LETTER THORN
        return wchar_t(uint16(wchar) + 0x0020);
    if (wchar >= 0x0100 && wchar <= 0x012E)                 // LATIN CAPITAL LETTER A WITH MACRON - LATIN CAPITAL LETTER I WITH OGONEK (only %2=0)
    {
        if (wchar % 2 == 0)
            return wchar_t(uint16(wchar) + 0x0001);
    }
    if (wchar == 0x1E9E)                                    // LATIN CAPITAL LETTER SHARP S
        return wchar_t(0x00DF);
    if (wchar == 0x0401)                                    // CYRILLIC CAPITAL LETTER IO
        return wchar_t(0x0451);
    if (wchar >= 0x0410 && wchar <= 0x042F)                 // CYRILLIC CAPITAL LETTER A - CYRILLIC CAPITAL LETTER YA
        return wchar_t(uint16(wchar) + 0x0020);

    return wchar;
}

inline void wstrToUpper(std::wstring& str)
{
    std::transform(str.begin(), str.end(), str.begin(), wcharToUpper);
}

inline void wstrToLower(std::wstring& str)
{
    std::transform(str.begin(), str.end(), str.begin(), wcharToLower);
}

/// 根据俄语/西里尔语变格规则提取角色名字的词干主干部分
std::wstring GetMainPartOfName(const std::wstring& wname, uint32 declension);

/// UTF-8 转控制台本地编码（Windows OEM）
bool utf8ToConsole(const std::string& utf8str, std::string& conStr);
/// 控制台本地编码转 UTF-8
bool consoleToUtf8(const std::string& conStr, std::string& utf8str);
/// 大小写不敏感地判断 UTF-8 字符串是否包含指定宽字符串子串
bool Utf8FitTo(const std::string& str, const std::wstring& search);
/// 向文件流输出 UTF-8 格式化字符串（Windows 下自动转 OEM 编码）
void utf8printf(FILE* out, const char* str, ...);
/// va_list 版本的 UTF-8 格式化输出函数
void vutf8printf(FILE* out, const char* str, va_list* ap);

/// 校验字符串是否为合法 IPv4/IPv6 地址
bool IsIPAddress(char const* ipaddress);
/// 将当前进程 PID 写入指定文件
uint32 CreatePIDFile(const std::string& filename);

/// 将字节数组编码为大写十六进制字符串
void hexEncodeByteArray(uint8* bytes, uint32 arrayLen, std::string& result);

/// 将强类型枚举值安全转换为底层整型
template<typename E>
constexpr typename std::underlying_type<E>::type AsUnderlyingType(E enumValue)
{
    static_assert(std::is_enum<E>::value, "AsUnderlyingType can only be used with enums");
    return static_cast<typename std::underlying_type<E>::type>(enumValue);
}

#endif
