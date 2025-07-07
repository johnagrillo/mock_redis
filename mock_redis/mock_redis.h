// mock_redis.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <chrono>
#include <cstdarg>
#include <functional>
#include <hiredis/hiredis.h>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

extern bool isAuth;
// -------------------
// Types and Enums
// -------------------

struct BinaryValue
{
    std::string data;

    BinaryValue(const char* ptr, size_t len) : data(ptr, len) {}
};
enum class ArgType
{
    String,
    Int,
    Binary,
};
using ArgValue = std::variant<std::string, int, BinaryValue>;

inline redisReply* mock(const char* arg1)
{
    return nullptr;
}

inline redisReply* mock(const char* format, const char* arg1)
{
    return nullptr;
}

inline redisReply* mock(const char* format, const char* arg1, const char* areg2)
{
    return nullptr;
}

inline redisReply* mock(const char* format, const char* arg1, int arg2, const char* arg3)
{
    return nullptr;
}

inline redisReply* mock(const char* format, const char* arg1, const char* arg2, const char* arg3)
{
    return nullptr;
}

// Variadic mock redisCommand wrapper
template <typename... Args> inline redisReply* redisCommandT(void* context, const char* format, Args&&... args)
{
    std::ostringstream oss;
    oss << "[MOCK redisCommand called]\n";

    // Log all arguments
    (void)std::initializer_list<int>{(oss << "Arg: " << args << "\n", 0)...};
    std::cout << oss.str();

    redisReply* reply = nullptr;

    if constexpr (sizeof...(args) == 1)
    {
        reply = mock(const_cast<char*>(format), std::forward<Args>(args)...);
    }
    else if constexpr (sizeof...(args) == 2)
    {
        reply = mock(const_cast<char*>(format), std::forward<Args>(args)...);
    }
    else
    {
        reply = mock(const_cast<char*>(format), std::forward<Args>(args)...);
    }

    return reply;
}

// auto redisCommand(const char* name, ...) -> redisReply*;

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
// Forward declare makeCommandEntry before CommandRegistrar uses it
template <typename Tag> static auto makeCommandEntry() -> CommandInfo;
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

std::vector<ArgValue> parseVaList(va_list ap, const std::vector<ArgType>& argTypes);
void printResult(redisReply* reply);

/*
 * Command Framework for Mock Redis
 * --------------------------------
 *
 * This framework provides a way to define Redis-like commands in C++ using
 * strong typing and variadic argument parsing.
 *
 * Each command is represented by a "Tag" struct that defines:
 *   - A constexpr `format` string showing the command syntax (for debugging/logging).
 *   - A tuple type `ArgTypes` listing the expected argument types.
 *   - A static `call` method implementing the command's logic.
 *
 * The framework automatically:
 *   - Deduces argument types (strings or ints) from the Tag's ArgTypes tuple.
 *   - Parses variadic C-style arguments (`va_list`) to typed C++ arguments.
 *   - Calls the Tag's `call` method with typed arguments.
 *   - Wraps and returns the Redis reply (`redisReply*`).
 *
 * Key Types:
 *   - `ArgType` enum: distinguishes between string and int arguments.
 *   - `ArgValue`: variant holding either a `std::string` or an `int`.
 *   - `CommandInfo`: holds argument types and a handler callable.
 *   - `HandlerFunc`: a std::function taking `va_list` and returning `redisReply*`.
 *
 * Usage Example:
 * --------------
 * struct AuthCmd
 * {
 *   static constexpr const char* format = "AUTH %s";
 *   using ArgTypes = std::tuple<std::string>;
 *   static redisReply* call(const std::string& password)
 *   {
 *       if (password == "hunter2") {
 *           isAuth = true;
 *           return createOkStatusReply();
 *       }
 *       return createErrorReply("-ERR invalid password");
 *   }
 * };
 *
 *
 *
 * --- Code ---
 */

template <typename T> static constexpr auto deduceArgType() -> ArgType
{
    if constexpr (std::is_same_v<T, std::string>)
        return ArgType::String;
    else if constexpr (std::is_same_v<T, int>)
        return ArgType::Int;
    else if constexpr (std::is_same_v<T, BinaryValue>)
        return ArgType::Binary;
    else
        static_assert(sizeof(T) == 0, "Unsupported ArgType");
}

// Turn a tuple type into a vector<ArgType>
template <typename Tuple> static auto getArgTypes() -> std::vector<ArgType>
{
    constexpr size_t N = std::tuple_size_v<Tuple>;
    std::vector<ArgType> types;
    types.reserve(N);

    [&]<std::size_t... I>(std::index_sequence<I...>)
    { (types.push_back(deduceArgType<std::tuple_element_t<I, Tuple>>()), ...); }(std::make_index_sequence<N>{});

    return types;
}

// Unpack a parsed ArgValue vector into a tuple
template <typename Tuple> static auto unpackArgs(const std::vector<ArgValue>& args) -> Tuple
{
    return [&]<std::size_t... I>(std::index_sequence<I...>)
    {
        return std::make_tuple(std::get<std::tuple_element_t<I, Tuple>>(args[I])...);
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Core generator
template <typename Tag> static auto makeCommandEntry() -> CommandInfo
{
    using Tuple = typename Tag::ArgTypes;
    auto types = getArgTypes<Tuple>();

    HandlerFunc handler = [types](va_list ap) -> redisReply*
    {
        std::vector<ArgValue> args = parseVaList(ap, types);
        Tuple tup = unpackArgs<Tuple>(args);

        redisReply* reply = std::apply(Tag::call, tup);
        printResult(reply);
        return reply;
    };

    return CommandInfo{types, handler};
}
