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

#ifndef __UPDATEMASK_H
#define __UPDATEMASK_H

#include "Util/Errors.h"

// 对象更新字段位掩码容器（按 32 位块组织，标记哪些 UpdateField 发生了变化需要同步到客户端）
class UpdateMask
{
    public:
        UpdateMask() : mHasData(false), mCount(0), mBlocks(0), mUpdateMask(nullptr) { }
        UpdateMask(const UpdateMask& mask) : mUpdateMask(nullptr) { *this = mask; }

        ~UpdateMask()
        {
            delete[] mUpdateMask;
        }

        // 置位指定字段索引的更新标志并标记存在待同步数据
        void SetBit(uint32 index)
        {
            ((uint8*)mUpdateMask)[ index >> 3 ] |= 1 << (index & 0x7);
            mHasData = true;
        }

        // 清除指定字段索引的更新标志
        void UnsetBit(uint32 index)
        {
            ((uint8*)mUpdateMask)[ index >> 3 ] &= (0xff ^ (1 << (index & 0x7)));
        }

        // 查询指定字段索引的更新标志是否已置位
        bool GetBit(uint32 index) const
        {
            return (((uint8*)mUpdateMask)[ index >> 3 ] & (1 << (index & 0x7))) != 0;
        }

        uint32 GetBlockCount() const { return mBlocks; }    // 返回 32 位掩码块数量
        uint32 GetLength() const { return mBlocks << 2; }   // 返回掩码总字节长度
        uint32 GetCount() const { return mCount; }          // 返回跟踪的字段总数
        uint8* GetMask() const { return (uint8*)mUpdateMask; } // 返回掩码底层字节指针
        bool HasData() const { return mHasData; }           // 是否存在已置位的字段

        // 分配并清零对应字段数量的掩码数组（每 32 个字段占 1 个 uint32 块）
        void SetCount(uint32 valuesCount)
        {
            delete[] mUpdateMask;

            mCount = valuesCount;
            mBlocks = (valuesCount + 31) / 32;

            mUpdateMask = new uint32[mBlocks];
            memset(mUpdateMask, 0, mBlocks << 2);
        }

        // 清零全部掩码位并重置数据标志
        void Clear()
        {
            if (mUpdateMask)
                memset(mUpdateMask, 0, mBlocks << 2);
            mHasData = false;
        }

        // 深拷贝赋值运算符
        UpdateMask& operator = (const UpdateMask& mask)
        {
            SetCount(mask.mCount);
            memcpy(mUpdateMask, mask.mUpdateMask, mBlocks << 2);

            return *this;
        }

        // 按位与复合赋值（用于与视野权限掩码求交集）
        void operator &= (const UpdateMask& mask)
        {
            MANGOS_ASSERT(mask.mCount <= mCount);
            for (uint32 i = 0; i < mBlocks; ++i)
                mUpdateMask[i] &= mask.mUpdateMask[i];
        }

        // 按位或复合赋值
        void operator |= (const UpdateMask& mask)
        {
            MANGOS_ASSERT(mask.mCount <= mCount);
            for (uint32 i = 0; i < mBlocks; ++i)
                mUpdateMask[i] |= mask.mUpdateMask[i];
        }

        // 按位与运算（返回新掩码）
        UpdateMask operator & (const UpdateMask& mask) const
        {
            MANGOS_ASSERT(mask.mCount <= mCount);

            UpdateMask newmask = *this;
            newmask &= mask;

            return newmask;
        }

        // 按位或运算（返回新掩码）
        UpdateMask operator | (const UpdateMask& mask) const
        {
            MANGOS_ASSERT(mask.mCount <= mCount);

            UpdateMask newmask = *this;
            newmask |= mask;

            return newmask;
        }

    private:
        bool mHasData;                                      // 是否至少有一位被设置过
        uint32 mCount;                                      // 字段总数
        uint32 mBlocks;                                     // 32 位块数
        uint32* mUpdateMask;                                // 动态分配的掩码数组
};
#endif
