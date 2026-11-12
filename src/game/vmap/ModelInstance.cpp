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

#include "ModelInstance.h"
#include "WorldModel.h"
#include "MapTree.h"
#include "VMapDefinitions.h"

using G3D::Vector3;
using G3D::Ray;

namespace VMAP
{
    // 构造模型实例：预计算旋转逆矩阵与缩放倒数
    ModelInstance::ModelInstance(const ModelSpawn& spawn, WorldModel* model): ModelSpawn(spawn), iModel(model)
    {
        iInvRot = G3D::Matrix3::fromEulerAnglesZYX(G3D::pi() * iRot.y / 180.f, G3D::pi() * iRot.x / 180.f, G3D::pi() * iRot.z / 180.f).inverse();
        iInvScale = 1.f / iScale;
    }

    // 射线求交：先测世界空间 AABB，再将射线变换到模型局部坐标调用 WorldModel::IntersectRay
    bool ModelInstance::intersectRay(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit, bool ignoreM2Model) const
    {
        if (!iModel)
        {
#ifdef VMAP_DEBUG
            DEBUG_LOG("<object not loaded>");
#endif
            return false;
        }
        float time = pRay.intersectionTime(iBound);
        if (time == G3D::inf())
        {
#ifdef VMAP_DEBUG
            DEBUG_LOG("Ray does not hit '%s'", name.c_str());
#endif
            return false;
        }
        // child bounds are defined in object space: (将射线起点与方向变换到局部空间)
        Vector3 p = iInvRot * (pRay.origin() - iPos) * iInvScale;
        Ray modRay(p, iInvRot * pRay.direction());
        float distance = pMaxDist * iInvScale;
        bool hit = iModel->IntersectRay(modRay, distance, pStopAtFirstHit, ignoreM2Model);
        if (hit)
        {
            distance *= iScale;
            pMaxDist = distance;
        }
        return hit;
    }

    // 空间点查询 WMO 区域信息及地面高度（跳过 M2 装饰模型）
    void ModelInstance::intersectPoint(const G3D::Vector3& p, AreaInfo& info) const
    {
        if (!iModel)
        {
#ifdef VMAP_DEBUG
            DEBUG_LOG("<object not loaded>");
#endif
            return;
        }

        // M2 files don't contain area info, only WMO files (仅 WMO 建筑包含区域标志信息，M2 不包含)
        if (flags & MOD_M2)
            return;
        if (!iBound.contains(p))
            return;
        // child bounds are defined in object space:
        Vector3 pModel = iInvRot * (p - iPos) * iInvScale;
        Vector3 zDirModel = iInvRot * Vector3(0.f, 0.f, -1.f);
        float zDist;
        if (iModel->IntersectPoint(pModel, zDirModel, zDist, info))
        {
            Vector3 modelGround = pModel + zDist * zDirModel;
            // Transform back to world space. Note that:
            // Mat * vec == vec * Mat.transpose()
            // and for rotation matrices: Mat.inverse() == Mat.transpose()
            // 利用正交旋转矩阵逆等于转置的性质，将局部地面点反变换回世界高度 Z
            float world_Z = ((modelGround * iInvRot) * iScale + iPos).z;
            if (info.ground_Z < world_Z)
            {
                info.ground_Z = world_Z;
                info.adtId = adtId;
            }
        }
    }

    // 查询空间点命中的组模型及最高有效地面位置信息
    bool ModelInstance::GetLocationInfo(const G3D::Vector3& p, LocationInfo& info) const
    {
        if (!iModel)
        {
#ifdef VMAP_DEBUG
            DEBUG_LOG("<object not loaded>");
#endif
            return false;
        }

        // M2 files don't contain area info, only WMO files
        if (flags & MOD_M2)
            return false;
        if (!iBound.contains(p))
            return false;
        // child bounds are defined in object space:
        Vector3 pModel = iInvRot * (p - iPos) * iInvScale;
        Vector3 zDirModel = iInvRot * Vector3(0.f, 0.f, -1.f);
        float zDist;
        GroupLocationInfo groupInfo;
        if (iModel->GetLocationInfo(pModel, zDirModel, zDist, groupInfo))
        {
            Vector3 modelGround = pModel + zDist * zDirModel;
            // Transform back to world space. Note that:
            // Mat * vec == vec * Mat.transpose()
            // and for rotation matrices: Mat.inverse() == Mat.transpose()
            float world_Z = ((modelGround * iInvRot) * iScale + iPos).z;
            if (info.ground_Z < world_Z) // hm...could it be handled automatically with zDist at intersection?
            {
                info.rootId = groupInfo.rootId;
                info.hitModel = groupInfo.hitModel;
                info.ground_Z = world_Z;
                info.hitInstance = this;
                return true;
            }
        }
        return false;
    }

    // 查询当前命中组模型局部坐标处的液体高度并反变换为世界 Z 坐标
    bool ModelInstance::GetLiquidLevel(const G3D::Vector3& p, LocationInfo& info, float& liqHeight) const
    {
        // child bounds are defined in object space:
        Vector3 pModel = iInvRot * (p - iPos) * iInvScale;
        // Vector3 zDirModel = iInvRot * Vector3(0.f, 0.f, -1.f);
        float zDist;
        if (info.hitModel->GetLiquidLevel(pModel, zDist))
        {
            // calculate world height (zDist in model coords):
            liqHeight = (Vector3(pModel.x, pModel.y, zDist) * iInvRot * iScale + iPos).z;
            return true;
        }
        return false;
    }

    // 从二进制文件反序列化读取一条 ModelSpawn 记录
    bool ModelSpawn::readFromFile(FILE* rf, ModelSpawn& spawn)
    {
        uint32 check = 0, nameLen;
        check += fread(&spawn.flags, sizeof(uint32), 1, rf);
        // EoF?
        if (!check)
        {
            if (ferror(rf))
                ERROR_LOG("Error reading ModelSpawn!");
            return false;
        }
        check += fread(&spawn.adtId, sizeof(uint16), 1, rf);
        check += fread(&spawn.ID, sizeof(uint32), 1, rf);
        check += fread(&spawn.iPos, sizeof(float), 3, rf);
        check += fread(&spawn.iRot, sizeof(float), 3, rf);
        check += fread(&spawn.iScale, sizeof(float), 1, rf);
        const bool has_bound = (spawn.flags & MOD_HAS_BOUND) != 0;
        if (has_bound) // only WMOs have bound in MPQ, only available after computation
        {
            Vector3 bLow, bHigh;
            check += fread(&bLow, sizeof(float), 3, rf);
            check += fread(&bHigh, sizeof(float), 3, rf);
            spawn.iBound = G3D::AABox(bLow, bHigh);
        }
        check += fread(&nameLen, sizeof(uint32), 1, rf);
        if (check != uint32(has_bound ? 17 : 11))
        {
            ERROR_LOG("Error reading ModelSpawn!");
            return false;
        }
        char nameBuff[500];
        if (nameLen > 500) // file names should never be that long, must be file error
        {
            ERROR_LOG("Error reading ModelSpawn, file name too long!");
            return false;
        }
        check = fread(nameBuff, sizeof(char), nameLen, rf);
        if (check != nameLen)
        {
            ERROR_LOG("Error reading name string of ModelSpawn!");
            return false;
        }
        spawn.name = std::string(nameBuff, nameLen);
        return true;
    }

    // 将一条 ModelSpawn 记录序列化写入二进制文件
    bool ModelSpawn::writeToFile(FILE* wf, const ModelSpawn& spawn)
    {
        uint32 check = 0;
        check += fwrite(&spawn.flags, sizeof(uint32), 1, wf);
        check += fwrite(&spawn.adtId, sizeof(uint16), 1, wf);
        check += fwrite(&spawn.ID, sizeof(uint32), 1, wf);
        check += fwrite(&spawn.iPos, sizeof(float), 3, wf);
        check += fwrite(&spawn.iRot, sizeof(float), 3, wf);
        check += fwrite(&spawn.iScale, sizeof(float), 1, wf);
        const bool has_bound = (spawn.flags & MOD_HAS_BOUND) != 0;
        if (has_bound) // only WMOs have bound in MPQ, only available after computation
        {
            check += fwrite(&spawn.iBound.low(), sizeof(float), 3, wf);
            check += fwrite(&spawn.iBound.high(), sizeof(float), 3, wf);
        }
        uint32 nameLen = spawn.name.length();
        check += fwrite(&nameLen, sizeof(uint32), 1, wf);
        if (check != uint32(has_bound ? 17 : 11)) return false;
        check = fwrite(spawn.name.c_str(), sizeof(char), nameLen, wf);
        return check == nameLen;
    }
}
