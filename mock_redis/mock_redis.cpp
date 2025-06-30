// mock_redis.cpp : Defines the entry point for the application.
//
#include "mock_redis.h"

#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "hiredis.h"

// -------------------
// In-memory store & auth
// -------------------

bool isAuth = false;

// -------------------
// Parse va_list according to ArgTypes
// -------------------

// NOLINTBEGIN

std::vector<ArgValue> parseVaList(va_list ap, const std::vector<ArgType>& argTypes)
{
    std::vector<ArgValue> result;
    for (ArgType type : argTypes)
    {
        switch (type)
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
        case ArgType::Binary:
        {
            const char* ptr = va_arg(ap, const char*);
            int len = va_arg(ap, int); // get the binary length
            result.emplace_back(BinaryValue(ptr, len));
            break;
        }
        }
    }
    return result;
}

// NOLINTEND

void printResult(redisReply* reply)
{
    if (reply == nullptr)
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

// -------------------
// Command dispatcher
// -------------------

auto redisCommandFromVaList(const char* name, va_list ap) -> redisReply*
{
    std::cerr << "hello2 " << name << "\n";
    auto& registry = CommandRegistry::get();
    if (!registry.contains(name))
    {
        std::cout << "-ERR unknown command '" << name << "'\n";
        return nullptr;
    }
    std::vprintf(name, ap);
    std::cout << "\n";
    


    auto& cmdInfo = registry.at(name);
    /*
    // Debugging the arguments passed to the command
    std::cout << name << " ";
    for (ArgType const t : cmdInfo.argTypes)
    {
        std::cout << " ";
        switch (t)
        {
        case ArgType::String:
        {
            const char* s = va_arg(ap, const char*);
            std::cout << "'" << s << "'";
            break;
        }
        case ArgType::Int:
        {
            int i = va_arg(ap, int);
            std::cout << i;
            break;
        }
        case ArgType::Binary:
        {
            const void* data = va_arg(ap, const void*);
            size_t length = va_arg(ap, size_t); // Assuming length is passed after the binary data
            std::cout << "[binary data, length: " << length << "]";
            break;
        }
        default: std::cout << "?";
        }
    }
    */
    // Call the command's handler function with the original va_list
    redisReply* result = cmdInfo.handler(ap);
    
    return result;
}

static auto redisCommand(const char* name, ...) -> redisReply*
{
    std::cerr << "hello " << name << "\n";
    va_list ap;
    va_start(ap, name);
    

    va_list ap_copy;
    va_copy(ap_copy, ap); // Copy ap to ap_copy
    auto *r = redisCommandFromVaList(name, ap_copy);
    va_end(ap_copy);

    va_end(ap);



    return r;

    //auto& registry = CommandRegistry::get();
    //if (!registry.contains(name))
    //{
    //    std::cout << "-ERR unknown command '" << name << "'\n";
    //    return nullptr;
    //}

    //auto& cmdInfo = registry.at(name);
    //redisReply* result = cmdInfo.handler(ap);
    //va_end(ap);
    //return result;
}

// -------------------
// Main function to test
// -------------------

static auto Set(const std::string& key, const std::string& value)
{
    auto* r = redisCommand("SET %s %s", key.c_str(), value.c_str());
    std::cout << " reply " << r->type << "\n";
}

static auto Get(const std::string& key)
{
    auto* r = redisCommand("GET %s", key.c_str());
    std::cout << " reply " << r->type << "\n";
    //std::cout << " reply " << r->str << "\n";
}

auto main() -> int
{

    // Sample set up for SMEMBERS
    // setDb["myset"] = {"one", "two", "three"};

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

// Modified create function returning unique_ptr
auto createRedisReply() -> redisReply*
{
    auto* reply = (redisReply*)calloc(1, sizeof(redisReply));
    std::cerr << "create " << reply << "\n";
    return reply;
}

// Modified create function returning unique_ptr
static auto createRedisReplyPtr() -> RedisReplyPtr
{
    auto* rawReply = (redisReply*)calloc(1, sizeof(redisReply));
    std::cerr << "create " << rawReply << "\n";

    return RedisReplyPtr(rawReply, &freeReplyObject);
}

auto createStatusReply(const char* status) -> redisReply*
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_STATUS;
    reply->str = _strdup(status); // heap-allocated copy
    reply->len = strlen(status);
    return reply;
}

auto createOkStatusReply() -> redisReply*
{
    return createStatusReply("+OK");
}

auto createErrorReply(const char* error) -> redisReply*
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_ERROR;
    reply->str = _strdup(error); // heap-allocated copy
    reply->len = strlen(error);
    return reply;
}

auto createAuthErrorReply() -> redisReply*
{
    return createErrorReply("-NOAUTH Authentication required");
}

auto createNilReply() -> redisReply*
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_NIL;
    return reply;
}

auto createStringReply(const std::string& s) -> redisReply*
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_STRING;
    reply->str = _strdup(s.c_str());
    reply->len = s.size();
    return reply;
}

auto createIntegerReply(int value) -> redisReply*
{
    redisReply* reply = createRedisReply();
    reply->type = REDIS_REPLY_INTEGER;
    reply->integer = value;
    return reply;
}
auto createArrayReply(size_t count) -> redisReply*
{
    auto* reply = createRedisReply();
    reply->type = REDIS_REPLY_ARRAY;
    reply->elements = count;
    reply->element = (redisReply**)calloc(count, sizeof(redisReply*));
    return reply;
}
