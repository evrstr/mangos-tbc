//This file included in Anticheat.hpp. This private table information
// 此文件被包含在 Anticheat.hpp 中，定义反作弊模块的私有 GM 聊天子命令表

// 指纹查询子命令表（.anticheat fingerprint ...）
static ChatCommand anticheatFingerprintCommandTable[] =
{
    { "list",      SEC_ADMINISTRATOR, false, &ChatHandler::HandleAnticheatFingerprintListCommand,      "", nullptr },
    { "history",   SEC_ADMINISTRATOR, false, &ChatHandler::HandleAnticheatFingerprintHistoryCommand,      "", nullptr },
    { "ahistory",  SEC_ADMINISTRATOR, false, &ChatHandler::HandleAnticheatFingerprintAHistoryCommand,      "", nullptr },
    { nullptr,   0,                  false, nullptr,                                             "", nullptr },
};

static ChatCommand anticheatCommandTable[] =
{
    { "info",         SEC_GAMEMASTER,    true,  &ChatHandler::HandleAnticheatInfoCommand,           "", nullptr },
    { "enable",       SEC_ADMINISTRATOR, true,  &ChatHandler::HandleAnticheatEnableCommand,         "", nullptr },
    { "silence",      SEC_GAMEMASTER,    false, &ChatHandler::HandleAnticheatSilenceCommand,        "", nullptr },
    { "unsilence",    SEC_GAMEMASTER,    false, &ChatHandler::HandleAnticheatUnsilenceCommand,      "", nullptr },
    { "spaminfo",     SEC_ADMINISTRATOR, false, &ChatHandler::HandleAnticheatSpaminfoCommand,       "", nullptr },
    { "fingerprint",  SEC_ADMINISTRATOR, false, nullptr,                                            "", anticheatFingerprintCommandTable },
    { "cheatinform",  SEC_GAMEMASTER,    false, &ChatHandler::HandleAnticheatCheatinformCommand,    "", nullptr },
    { "spaminform",   SEC_GAMEMASTER,    false, &ChatHandler::HandleAnticheatSpaminformCommand,     "", nullptr },
    { "blacklist",    SEC_GAMEMASTER,    false, &ChatHandler::HandleAnticheatBlacklistCommand,      "", nullptr },
    { "debugextrap",  SEC_ADMINISTRATOR, true,  &ChatHandler::HandleAnticheatDebugExtrapCommand,    "", nullptr },
    { nullptr,   0,                  false, nullptr,                                                "", nullptr },
};