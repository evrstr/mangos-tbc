/*
 * Copyright (C) 2010-2013 Anathema Engine project <http://valkyrie-wow.com/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2016-2017 Elysium Project <https://www.elysium-project.org>
 * Copyright (C) 2017-2020 namreeb (legal@namreeb.org)
 */


#include "Auth/CryptoHash.h"

#ifndef _WARDEN_KEY_GENERATION_H
#define _WARDEN_KEY_GENERATION_H

// Warden 会话密钥流生成器：基于客户端认证会话密钥 K 通过双半段 SHA-1 迭代派生 RC4 输入/输出密钥
class SHA1Randx
{
public:
    // 使用会话密钥缓冲区初始化内部两个半段摘要 o1、o2，并预生成首块输出 o0
    SHA1Randx(const uint8* buff, size_t size)
    {
        auto const taken = size/2;

        sh.Initialize();
        sh.UpdateData(buff, taken);
        sh.Finalize();

        memcpy(o1, sh.GetDigest(), 20);

        sh.Initialize();
        sh.UpdateData(buff + taken, size - taken);
        sh.Finalize();

        memcpy(o2, sh.GetDigest(), 20);

        memset(o0, 0x00, 20);

        FillUp();
    }

    // 连续输出指定长度 sz 的派生密钥字节流
    void Generate(uint8* buf, size_t sz)
    {
        for (uint32 i = 0; i < sz; ++i)
        {
            if (taken == 20)
                FillUp();

            buf[i] = o0[taken];
            taken++;
        }
    }

private:
    // 按 SHA1(o1 || o0 || o2) 刷新 20 字节密钥缓冲区并重置读取游标
    void FillUp()
    {
        sh.Initialize();
        sh.UpdateData(o1, 20);
        sh.UpdateData(o0, 20);
        sh.UpdateData(o2, 20);
        sh.Finalize();

        memcpy(o0, sh.GetDigest(), 20);

        taken = 0;
    }

    Sha1Hash sh;
    size_t taken;
    uint8 o0[20],o1[20],o2[20];
};

#endif
