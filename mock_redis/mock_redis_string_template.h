#pragma once

#include "mock_redis.h"

static std::unordered_map<std::string, std::pair<std::string, std::chrono::time_point<std::chrono::system_clock>>>
    strDb;

// Helper function to check expiration
bool isExpired(const std::chrono::time_point<std::chrono::system_clock>& expiryTime)
{
    return std::chrono::system_clock::now() > expiryTime;
}



struct ExistsCmd
{
    static constexpr const char* format = "EXISTS %s";
    using ArgTypes = std::tuple<std::string>;

    static redisReply* call(const std::string& key)
    {
        if (!isAuth)
            return createAuthErrorReply();

        auto it = strDb.find(key);
        if (it == strDb.end())
            return createIntegerReply(0);

        if (isExpired(it->second.second))
        {
            strDb.erase(it);
            return createIntegerReply(0);
        }

        return createIntegerReply(1);
    }

    inline static const AutoRegister<ExistsCmd> trigger{};
};

struct ExpireCmd
{
    static constexpr const char* format = "EXPIRE %s %d";
    using ArgTypes = std::tuple<std::string, int>;

    static redisReply* call(const std::string& key, int seconds)
    {
        if (!isAuth)
            return createAuthErrorReply();

        auto it = strDb.find(key);
        if (it == strDb.end())
            return createIntegerReply(0);

        if (isExpired(it->second.second))
        {
            strDb.erase(it);
            return createIntegerReply(0);
        }

        it->second.second = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
        return createIntegerReply(1);
    }

    inline static const AutoRegister<ExpireCmd> trigger{};
};

struct GetCmd
{
    static constexpr const char* format = "GET %s";
    using ArgTypes = std::tuple<std::string>;

    static redisReply* call(const std::string& key)
    {
        if (!isAuth)
            return createAuthErrorReply();

        auto it = strDb.find(key);
        if (it == strDb.end())
            return createNilReply();

        if (isExpired(it->second.second))
        {
            strDb.erase(it);
            return createNilReply();
        }

        return createStringReply(it->second.first);
    }

    inline static const AutoRegister<GetCmd> trigger{};
};

struct SetCmd
{
    static constexpr const char* format = "SET %s %s";
    using ArgTypes = std::tuple<std::string, std::string>;

    static redisReply* call(const std::string& key, const std::string& val)
    {
        if (!isAuth)
            return createAuthErrorReply();

        strDb[key] = {val, std::chrono::time_point<std::chrono::system_clock>::max()};
        return createOkStatusReply();
    }

    inline static const AutoRegister<SetCmd> trigger{};
};

struct SetExCmd
{
    static constexpr const char* format = "SETEX %s %d %s";
    using ArgTypes = std::tuple<std::string, int, std::string>;

    static redisReply* call(const std::string& key, int seconds, const std::string& val)
    {
        if (!isAuth)
            return createAuthErrorReply();

        strDb[key] = {val, std::chrono::system_clock::now() + std::chrono::seconds(seconds)};
        return createOkStatusReply();
    }

    inline static const AutoRegister<SetExCmd> trigger{};
};

struct TTLCmd
{
    static constexpr const char* format = "TTL %s";
    using ArgTypes = std::tuple<std::string>;

    static redisReply* call(const std::string& key)
    {
        if (!isAuth)
            return createAuthErrorReply();

        auto it = strDb.find(key);
        if (it == strDb.end())
            return createIntegerReply(-2);

        if (it->second.second == std::chrono::time_point<std::chrono::system_clock>::max())
            return createIntegerReply(-1);

        if (isExpired(it->second.second))
        {
            strDb.erase(it);
            return createIntegerReply(-2);
        }

        auto now = std::chrono::system_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second.second - now).count();

        return createIntegerReply(static_cast<int>(remaining));
    }

    inline static const AutoRegister<TTLCmd> trigger{};
};
