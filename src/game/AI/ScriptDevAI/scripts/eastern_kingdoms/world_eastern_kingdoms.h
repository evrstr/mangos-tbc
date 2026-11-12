
// 东部王国跨区域全局头文件定义

#ifndef DEF_WORLD_MAP_EK_SCRIPTS_H                          // 防止头文件重复包含
#define DEF_WORLD_MAP_EK_SCRIPTS_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    // TYPE_SHADE_OF_HEADLESS_HORSEMAN = 1000, // High number against collisions

    // Quest 6403 (任务6403)
    NPC_JONATHAN                = 466,                      // jonathan Entry ID
    NPC_WRYNN                   = 1747,                     // wrynn Entry ID
    NPC_BOLVAR                  = 1748,                     // bolvar Entry ID
    NPC_PRESTOR                 = 1749,                     // prestor Entry ID
    NPC_WINDSOR                 = 12580,                    // 温德索尔 Entry ID

    NPC_FALSTAD_WILDHAMMER      = 5635,                     // falstadwildhammer Entry ID

    NPC_SHORT_JOHN_MITHRIL      = 14508,                    // shortjohnmithril Entry ID

    // Quest: Stranglethorn Fever Waves
    STV_FEVER_GROUP_01          = 18011,                    // fevergroup01 标识 ID
    STV_FEVER_GROUP_02          = 18012,                    // fevergroup02 标识 ID
    STV_FEVER_GROUP_03          = 18013,                    // fevergroup03 标识 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum BFTDP_NPCs
{
    // Attacking forces
    NPC_DREADKNIGHT             = 19288,                    // dreadknight Entry ID
    NPC_INVADING_FELGUARD       = 19284,                    // invadingfelguard Entry ID
    NPC_INVADING_VOIDWALKER     = 19287,                    // invadingvoidwalker Entry ID
    NPC_INVADING_INFERNAL       = 19285,                    // invadinginfernal Entry ID
    NPC_PORTAL_HOUND            = 19311,                    // 传送门hound Entry ID
    NPC_FELGUARD_LIEUTENANT     = 19391,                    // felguardlieutenant Entry ID
    NPC_INVADING_FEL_STALKER    = 19286,                    // invading魔能stalker Entry ID
    NPC_INVADING_ANGUISHER      = 19290,                    // invadinganguisher Entry ID

    // Defending forces
    NPC_ARGENT_GUARDIAN         = 19322,                    // argentguardian Entry ID
    NPC_ARGENT_BOWMAN           = 19365,                    // argentbowman Entry ID
    NPC_ARGENT_PROTECTOR        = 19320,                    // argentprotector Entry ID
    NPC_ARGENT_HUNTER           = 19366,                    // argent猎人 Entry ID
    NPC_NETHERGARDE_ADVISOR     = 19566,                    // nethergardeadvisor Entry ID
    NPC_AGENT_PROUDWELL         = 19942,                    // agentproudwell Entry ID
    NPC_WATCHER_THERONUS        = 19567,                    // 观察者theronus Entry ID
    NPC_SILVERMOON_MAGISTER     = 19006,                    // silvermoonmagister Entry ID
    NPC_LORD_MARSHAL_RAYNOR     = 19385,                    // lordmarshalraynor Entry ID
    NPC_STORMWIND_MARSHAL       = 19386,                    // stormwindmarshal Entry ID
    NPC_JUSTINIUS_THE_HARBINGER = 18966,                    // justiniustheharbinger Entry ID
    NPC_AZUREMYST_VINDICATOR    = 19407,                    // azuremystvindicator Entry ID
    NPC_MELGROMM_HIGHMOUNTAIN   = 18969,                    // melgrommhighmountain Entry ID
    NPC_THUNDER_BLUFF_HUNTSMAN  = 19406,                    // thunderbluffhuntsman Entry ID
    NPC_GNOMEREGAN_CONJUROR     = 19007,                    // gnomereganconjuror Entry ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum BFTDP_Spells
{
    SPELL_SUMMON_INFERNALING_PORTAL_HOUND = 33708           // 召唤infernaling传送门hound 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum BFTDP_Texts
{
    LMR_SPAWN           = -1015080,                         // 生成 标识 ID
    LMR_RANDOM_TEXT_1   = -1015081,                         // randomtext1 标识 ID
    LMR_RANDOM_TEXT_2   = -1015082,                         // randomtext2 标识 ID
    LMR_RANDOM_TEXT_3   = -1015083,                         // randomtext3 标识 ID
    LMR_RANDOM_TEXT_4   = -1015084,                         // randomtext4 标识 ID
    LMR_RANDOM_TEXT_5   = -1015085,                         // randomtext5 标识 ID
    LMR_DESPAWN         = -1015086,                         // 消除 标识 ID

    JTH_SPAWN           = -1015087,                         // 生成 标识 ID
    JTH_RANDOM_TEXT_1   = -1015088,                         // randomtext1 标识 ID
    JTH_RANDOM_TEXT_2   = -1015089,                         // randomtext2 标识 ID
    JTH_RANDOM_TEXT_3   = -1015090,                         // randomtext3 标识 ID
    JTH_DESPAWN         = -1015091,                         // 消除 标识 ID

    MH_SPAWN            = -1015092,                         // 生成 标识 ID
    MH_RANDOM_TEXT_1    = -1015093,                         // randomtext1 标识 ID
    MH_RANDOM_TEXT_2    = -1015094,                         // randomtext2 标识 ID
    MH_RANDOM_TEXT_3    = -1015095,                         // randomtext3 标识 ID
    MH_RANDOM_TEXT_4    = -1015096,                         // randomtext4 标识 ID
    MH_RANDOM_TEXT_5    = -1015097,                         // randomtext5 标识 ID
    MH_RANDOM_TEXT_6    = -1015098,                         // randomtext6 标识 ID
    MH_DESPAWN          = -1015099,                         // 消除 标识 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum BFTDP_Events
{
    EVENT_SPAWN_DREADKNIGHT = 1,                            // 生成dreadknight 事件标识 ID
    EVENT_SPAWN_FELGUARD_LIEUTENANT = 2,                    // 生成felguardlieutenant 事件标识 ID
    EVENT_SPAWN = 3,                                        // 生成 事件标识 ID
    EVENT_SPAWN_BOSS = 4,                                   // 生成boss 事件标识 ID
    EVENT_REINFORCEMENTS_NEEDED = 5,                        // reinforcementsneeded 事件标识 ID
    EVENT_SPAWN_REINFORCEMENTS = 6,                         // 生成reinforcements 事件标识 ID
    EVENT_REINFORCEMENTS_LEADER_SPEAK = 7,                  // reinforcementsleaderspeak 事件标识 ID
};

#endif                                                      // 结束头文件防重复包含宏
