//This file included in Chat.h in class header
// 此文件被包含在 Chat.h 的 ChatHandler 类声明中，声明反作弊相关 GM 命令处理函数

bool HandleAnticheatInfoCommand(char* args);
bool HandleAnticheatEnableCommand(char* args);
bool HandleAnticheatSilenceCommand(char* args);
bool HandleAnticheatUnsilenceCommand(char* args);
bool HandleAnticheatSpaminfoCommand(char* args);
bool HandleAnticheatCheatinformCommand(char* args);
bool HandleAnticheatSpaminformCommand(char* args);
bool HandleAnticheatBlacklistCommand(char* args);
bool HandleAnticheatDebugExtrapCommand(char* args);

//fingerprint commands (系统指纹查询命令)
bool HandleAnticheatFingerprintListCommand(char* args);
bool HandleAnticheatFingerprintHistoryCommand(char* args);
bool HandleAnticheatFingerprintAHistoryCommand(char* args);