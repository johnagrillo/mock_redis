// mock_redis.cpp : Defines the entry point for the application.
//
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
// In-memory store & auth
// -------------------

static std::unordered_map<std::string, std::unordered_set<std::string>> setDb;

static bool isAuth = false;

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





#include "mock_redis_hash.h"
#include "mock_redis_misc.h"
#include "mock_redis_set.h"
#include "mock_redis_string.h"

// -------------------
// Parse va_list according to ArgTypes
// -------------------

std::vector<ArgValue> parseVaList(va_list ap, const std::vector<ArgType>& argTypes)
{
    std::vector<ArgValue> result;
    for (ArgType t : argTypes)
    {
        switch (t)
        {
        case ArgType::String:
        {
            const char* s = va_arg(ap, const char*);
            result.emplace_back(std::string(s));
            break;
        }
        case ArgType::Int:
        {
            int i = va_arg(ap, int);
            result.emplace_back(i);
            break;
        }
        default: throw std::runtime_error("Unknown ArgType");
        }
    }
    return result;
}

// Utility: overloaded lambda for std::visit
template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void printResult(redisReply* reply)
{
    if (!reply)
    {
        std::cout << "-ERR No reply\n";
        return;
    }

    switch (reply->type)
    {
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_STRING: std::cout << reply->str << "\n"; break;
    case REDIS_REPLY_ERROR: std::cout << reply->str << "\n"; break;
    case REDIS_REPLY_NIL:
        std::cout << "$-1\n"; // Redis-style nil
        break;
    case REDIS_REPLY_ARRAY:
        std::cout << "*" << reply->elements << "\n";
        for (size_t i = 0; i < reply->elements; i++)
        {
            std::cout << "$" << reply->element[i]->len << "\n" << reply->element[i]->str << "\n";
        }
        break;
    case REDIS_REPLY_INTEGER: std::cout << ":" << reply->integer << "\n"; break;
    default: std::cout << "-ERR Unknown reply type\n";
    }
}


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

// Deduce ArgType for a C++ type
template <typename T> constexpr ArgType deduceArgType()
{
    if constexpr (std::is_same_v<T, std::string>)
        return ArgType::String;
    else if constexpr (std::is_same_v<T, int>)
        return ArgType::Int;
    else
        static_assert(sizeof(T) == 0, "Unsupported ArgType");
}

// Turn a tuple type into a vector<ArgType>
template <typename Tuple> std::vector<ArgType> getArgTypes()
{
    constexpr size_t N = std::tuple_size_v<Tuple>;
    std::vector<ArgType> types;
    types.reserve(N);

    [&]<std::size_t... I>(std::index_sequence<I...>)
    { (types.push_back(deduceArgType<std::tuple_element_t<I, Tuple>>()), ...); }(std::make_index_sequence<N>{});

    return types;
}

// Unpack a parsed ArgValue vector into a tuple
template <typename Tuple> Tuple unpackArgs(const std::vector<ArgValue>& args)
{
    return [&]<std::size_t... I>(std::index_sequence<I...>)
    {
        return std::make_tuple(std::get<std::tuple_element_t<I, Tuple>>(args[I])...);
    }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Core generator
template <typename Tag> CommandInfo makeCommandEntry()
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

// -------------------
// Command dispatcher
// -------------------

redisReply* redisCommand(const std::string name, ...)
{

    auto registry = CommandRegistry::get();
    if (!registry.contains(name))
    {
        std::cout << "-ERR unknown command '" << name << "'\n";
        return nullptr; // Return nullptr if the command is not found
    }

    auto it = registry.at(name);

    va_list ap;
    va_start(ap, name);

    // Make a copy of va_list for printing arguments (optional)
    va_list ap_copy;
    va_copy(ap_copy, ap);
    std::cout << name << " ";
    // Print command name and args
    for (ArgType t : it.argTypes)
    {
        std::cout << " ";
        switch (t)
        {
        case ArgType::String:
        {
            const char* s = va_arg(ap_copy, const char*);
            std::cout << "'" << s << "'";
            break;
        }
        case ArgType::Int:
        {
            int i = va_arg(ap_copy, int);
            std::cout << i;
            break;
        }
        default: std::cout << "?";
        }
    }
    std::cout << "\n";

    // Call the handler and return the result
    redisReply* result = it.handler(ap);
    va_end(ap);
    return result; // Return the redisReply pointer
}

// -------------------
// Main function to test
// -------------------

auto Set(std::string key, std::string value)
{
    auto* r = redisCommand("SET %s %s", key.c_str(), value.c_str());
    std::cout << " reply " << r->type << "\n";
}

auto Get(std::string key)
{
    auto* r = redisCommand("GET %s", key.c_str());
    std::cout << " reply " << r->type << "\n";
    std::cout << " reply " << r->str << "\n";
}

int main()
{

    auto& registry = CommandRegistry::get();

    std::cout << "Registered commands:\n";

    for (const auto& [format, info] : registry)
    {
        std::cout << format << "\n";
    }

    redisCommand("AUTH %s", "hunter2");
    Set("key", "value");
    Get("key");
    Get("none");

    return 0;
}

int test()
{

    // Sample set up for SMEMBERS
    setDb["myset"] = {"one", "two", "three"};

    // Using the formatted command dispatcher
    auto* r = redisCommand("AUTH %s", "hunter2"); // AUTH hunter2 -> +OK
    std::cout << r->str << "\n";
    redisCommand("SET %s %s", "mykey", "myvalue"); // SET mykey myvalue -> +OK
    redisCommand("GET %s", "mykey");               // GET mykey -> prints value
    redisCommand("PING");                          // PING -> +PONG
    redisCommand("GET %s", "nokey");               // GET nokey -> $-1

    // Test other commands with formatted string
    redisCommand("SMEMBERS %s", "myset");    // SMEMBERS myset -> list of members
    redisCommand("SET %s %s", "foo", "bar"); // SET foo bar -> +OK
    redisCommand("AUTH %s", "badpass");      // AUTH badpass -> -ERR invalid password
    redisCommand("PING");                    // PING -> -NOAUTH

    // --- TEST HSET/HGET ---
    redisCommand("HSET %s %s %s", "myhash", "field1", "hello"); // :1
    redisCommand("HGET %s %s", "myhash", "field1");             // hello
    redisCommand("HGET %s %s", "myhash", "nofield");            // $-1
    redisCommand("HGET %s %s", "nokey", "field1");              // $-1

    // --- TEST SADD ---
    redisCommand("SADD %s %s", "myset", "four"); // :1 (new element)
    redisCommand("SADD %s %s", "myset", "two");  // :0 (already exists)
    redisCommand("SMEMBERS %s", "myset");        // one, two, three, four

    // --- TEST SREM ---
    redisCommand("SREM %s %s", "myset", "two");      // :1 (removed)
    redisCommand("SREM %s %s", "myset", "notfound"); // :0 (not present)
    redisCommand("SMEMBERS %s", "myset");            // one, three, four

    return 0;
}

// Modified create function returning unique_ptr
auto createRedisReply() -> redisReply*
{
    redisReply* reply = (redisReply*)calloc(1, sizeof(redisReply));
    std::cerr << "create " << reply << "\n";
    return reply;
}

// Modified create function returning unique_ptr
auto createRedisReplyPtr() -> RedisReplyPtr
{
    redisReply* rawReply = (redisReply*)calloc(1, sizeof(redisReply));
    std::cerr << "create " << rawReply << "\n";

    return RedisReplyPtr(rawReply, &freeReplyObject);
}

redisReply* createStatusReply(const char* status)
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_STATUS;
    reply->str = _strdup(status); // heap-allocated copy
    reply->len = strlen(status);
    return reply;
}

redisReply* createOkStatusReply()
{
    return createStatusReply("+OK");
}

redisReply* createErrorReply(const char* error)
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_ERROR;
    reply->str = _strdup(error); // heap-allocated copy
    reply->len = strlen(error);
    return reply;
}

redisReply* createAuthErrorReply()
{
    return createErrorReply("-NOAUTH Authentication required");
}

redisReply* createNilReply()
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_NIL;
    return reply;
}

redisReply* createStringReply(const std::string& s)
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_STRING;
    reply->str = _strdup(s.c_str());
    reply->len = s.size();
    return reply;
}

redisReply* createIntegerReply(int value)
{
    redisReply* reply = createRedisReply();
    reply->type = REDIS_REPLY_INTEGER;
    reply->integer = value;
    return reply;
}
redisReply* createArrayReply(size_t count)
{
    redisReply* reply = (redisReply*)calloc(1, sizeof(redisReply));
    reply->type = REDIS_REPLY_ARRAY;
    reply->elements = static_cast<size_t>(count);
    reply->element = (redisReply**)calloc(count, sizeof(redisReply*));
    return reply;
}