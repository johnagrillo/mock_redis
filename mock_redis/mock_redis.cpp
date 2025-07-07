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

#include "hiredis/hiredis.h"

// Function to return the string representation of a Redis reply type
std::string getRedisReplyType(int replyType)
{
    switch (replyType)
    {
    case REDIS_REPLY_STRING: return "REDIS_REPLY_STRING";
    case REDIS_REPLY_ARRAY: return "REDIS_REPLY_ARRAY";
    case REDIS_REPLY_INTEGER: return "REDIS_REPLY_INTEGER";
    case REDIS_REPLY_NIL: return "REDIS_REPLY_NIL";
    case REDIS_REPLY_STATUS: return "REDIS_REPLY_STATUS";
    case REDIS_REPLY_ERROR: return "REDIS_REPLY_ERROR";
    case REDIS_REPLY_DOUBLE: return "REDIS_REPLY_DOUBLE";
    case REDIS_REPLY_BOOL: return "REDIS_REPLY_BOOL";
    case REDIS_REPLY_MAP: return "REDIS_REPLY_MAP";
    case REDIS_REPLY_SET: return "REDIS_REPLY_SET";
    case REDIS_REPLY_PUSH: return "REDIS_REPLY_PUSH";
    case REDIS_REPLY_BIGNUM: return "REDIS_REPLY_BIGNUM";
    case REDIS_REPLY_VERB: return "REDIS_REPLY_VERB";
    default: return "Unknown Redis reply type";
    }
}

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

#include <hiredis/hiredis.h>
#include <iostream>

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

    case REDIS_REPLY_ERROR: std::cout << "-ERR " << reply->str << "\n"; break;

    case REDIS_REPLY_NIL:
        std::cout << "$-1\n"; // Redis-style NIL
        break;

    case REDIS_REPLY_ARRAY:
        std::cout << "*" << reply->elements << "\n";
        for (size_t i = 0; i < reply->elements; ++i)
        {
            redisReply* elem = reply->element[i];
            if (elem->type == REDIS_REPLY_STRING || elem->type == REDIS_REPLY_STATUS)
            {
                std::cout << "$" << elem->len << "\n" << elem->str << "\n";
            }
            else if (elem->type == REDIS_REPLY_INTEGER)
            {
                std::cout << ":" << elem->integer << "\n";
            }
            else if (elem->type == REDIS_REPLY_NIL)
            {
                std::cout << "$-1\n";
            }
            else
            {
                std::cout << "-ERR Unsupported array element type\n";
            }
        }
        break;

    case REDIS_REPLY_INTEGER: std::cout << ":" << reply->integer << "\n"; break;

    default: std::cout << "-ERR Unknown reply type: " << reply->type << "\n"; break;
    }
}

// -------------------
// Command dispatcher
// -------------------

auto redisCommandFromVaList(const char* name, va_list ap) -> redisReply*
{
    auto& registry = CommandRegistry::get();
    if (!registry.contains(name))
    {
        std::cout << "-ERR unknown command '" << name << "'\n";
        return nullptr;
    }

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

auto redisCommandM(const char* name, ...) -> redisReply*
{
    va_list args;
    va_start(args, name);

    auto* r = redisCommandFromVaList(name, args);
    va_end(args);
    return r;

    // auto& registry = CommandRegistry::get();
    // if (!registry.contains(name))
    //{
    //     std::cout << "-ERR unknown command '" << name << "'\n";
    //     return nullptr;
    // }

    // auto& cmdInfo = registry.at(name);
    // redisReply* result = cmdInfo.handler(ap);
    // va_end(ap);
    // return result;
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
#ifdef _WIN32
    reply->str = _strdup(status); // heap-allocated copy
#else
    reply->str = strdup(status); // heap-allocated copy
#endif
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
#ifdef _WIN32
    reply->str = _strdup(error); // heap-allocated copy
#else
    reply->str = strdup(error); // heap-allocated copy
#endif

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
#ifdef _WIN32
    reply->str = _strdup(s.c_str());
#else
    reply->str = strdup(s.c_str());
#endif
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
    reply->len = count;
    reply->element = (redisReply**)calloc(count, sizeof(redisReply*));
    return reply;
}
