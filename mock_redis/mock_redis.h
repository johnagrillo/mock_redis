// mock_redis.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <chrono>
#include <cstdarg>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "hiredis.h"

// -------------------
// Types and Enums
// -------------------

enum class ArgType
{
    String,
    Int
};
using ArgValue = std::variant<std::string, int>;

static std::string argTypeName(ArgType t)
{
    return (t == ArgType::String) ? "string" : "int";
}

template <typename Tag> struct Command;

// Result can be nothing, a single string, or a list of strings
using CommandResult = redisReply*; // Now we are returning redisReply directly

// -------------------
// CommandInfo and command table
// -------------------

using HandlerFunc = std::function<redisReply*(va_list)>;

struct CommandInfo
{
    std::vector<ArgType> argTypes;
    HandlerFunc handler;
};

// Global command registry singleton
struct CommandRegistry
{
    // Returns the singleton command registry map
    static std::unordered_map<std::string, CommandInfo>& get()
    {
        static std::unordered_map<std::string, CommandInfo> instance;
        return instance;
    }
};

// CommandRegistrar helper template
template <typename Tag> struct CommandRegistrar
{
    CommandRegistrar()
    {
        // Register the command into the global registry by format string
        CommandRegistry::get()[Tag::format] = makeCommandEntry<Tag>();
    }
};

template <typename Tag> struct AutoRegister
{
    static inline CommandRegistrar<Tag> registrar{};
};

// TODO: Reference additional headers your program requires here.
redisReply* createRedisReply();


// Alias for unique_ptr with custom deleter
using RedisReplyPtr = std::unique_ptr<redisReply, decltype(&freeReplyObject)>;

redisReply* createStatusReply(const char* status);
redisReply* createOkStatusReply();
redisReply* createErrorReply(const char* error);
redisReply* createAuthErrorReply();
redisReply* createNilReply();
redisReply* createStringReply(const std::string& s);
redisReply* createIntegerReply(int value);
redisReply* createArrayReply(size_t count);
