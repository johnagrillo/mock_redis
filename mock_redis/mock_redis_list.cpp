#include "mock_redis.h"

static std::unordered_map<std::string,
                          std::pair<std::vector<std::string>, std::chrono::time_point<std::chrono::system_clock>>>
    listDb;


static bool isExpired(const std::chrono::time_point<std::chrono::system_clock>& expiryTime)
{
    return std::chrono::system_clock::now() > expiryTime;
}

// ---------
//  LPUSH CMD
// ---------
struct LPushCmd
{
    static constexpr const char* tag = "LPUSH";
    static constexpr const char* format = "LPUSH %s %s";
    using ArgTypes = std::tuple<std::string, std::string>;

    static CommandResult call(const std::string& key, const std::string& val)
    {
        if (!isAuth) return createAuthErrorReply();

        auto& [list, expiry] = listDb[key];
        if (expiry == std::chrono::time_point<std::chrono::system_clock>{})
            expiry = std::chrono::time_point<std::chrono::system_clock>::max();

        list.insert(list.begin(), val);
        return createIntegerReply(static_cast<int>(list.size()));
    }

    static inline CommandRegistrar<LPushCmd> registrar{};
};

// ---------
//  RPUSH CMD
// ---------
struct RPushCmd
{
    static constexpr const char* tag = "RPUSH";
    static constexpr const char* format = "RPUSH %s %s";
    using ArgTypes = std::tuple<std::string, std::string>;

    static CommandResult call(const std::string& key, const std::string& val)
    {
        if (!isAuth) return createAuthErrorReply();

        auto& [list, expiry] = listDb[key];
        if (expiry == std::chrono::time_point<std::chrono::system_clock>{})
            expiry = std::chrono::time_point<std::chrono::system_clock>::max();

        list.push_back(val);
        return createIntegerReply(static_cast<int>(list.size()));
    }

    static inline CommandRegistrar<RPushCmd> registrar{};
};

// ---------
//  LPOP CMD
// ---------
struct LPopCmd
{
    static constexpr const char* tag = "LPOP";
    static constexpr const char* format = "LPOP %s";
    using ArgTypes = std::tuple<std::string>;

    static CommandResult call(const std::string& key)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!listDb.contains(key)) return createNilReply();

        auto& [list, expiry] = listDb.at(key);
        if (isExpired(expiry))
        {
            listDb.erase(key);
            return createNilReply();
        }

        if (list.empty()) return createNilReply();

        std::string front = list.front();
        list.erase(list.begin());

        return createStringReply(front);
    }

    static inline CommandRegistrar<LPopCmd> registrar{};
};

// ---------
//  RPOP CMD
// ---------
struct RPopCmd
{
    static constexpr const char* tag = "RPOP";
    static constexpr const char* format = "RPOP %s";
    using ArgTypes = std::tuple<std::string>;

    static CommandResult call(const std::string& key)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!listDb.contains(key)) return createNilReply();

        auto& [list, expiry] = listDb.at(key);
        if (isExpired(expiry))
        {
            listDb.erase(key);
            return createNilReply();
        }

        if (list.empty()) return createNilReply();

        std::string back = list.back();
        list.pop_back();

        return createStringReply(back);
    }

    static inline CommandRegistrar<RPopCmd> registrar{};
};

// ---------
//  LRANGE CMD
// ---------
struct LRangeCmd
{
    static constexpr const char* tag = "LRANGE";
    static constexpr const char* format = "LRANGE %s %d %d";
    using ArgTypes = std::tuple<std::string, int, int>;

    static CommandResult call(const std::string& key, int start, int stop)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!listDb.contains(key)) return createArrayReply(0);

        auto& [list, expiry] = listDb.at(key);
        if (isExpired(expiry))
        {
            listDb.erase(key);
            return createArrayReply(0);
        }

        int len = static_cast<int>(list.size());

        // Normalize negative indices
        if (start < 0) start = len + start;
        if (stop < 0) stop = len + stop;

        if (start < 0) start = 0;
        if (stop >= len) stop = len - 1;

        if (start > stop || start >= len) return createArrayReply(0);

        int count = stop - start + 1;
        redisReply* reply = createArrayReply(count);

        for (int i = 0; i < count; ++i)
        {
            const std::string& elem = list[start + i];
            reply->element[i] = createStringReply(elem);
        }

        return reply;
    }

    static inline CommandRegistrar<LRangeCmd> registrar{};
};

// ---------
//  LLEN CMD
// ---------
struct LLenCmd
{
    static constexpr const char* tag = "LLEN";
    static constexpr const char* format = "LLEN %s";
    using ArgTypes = std::tuple<std::string>;
    
    static CommandResult call(const std::string& key)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!listDb.contains(key)) return createIntegerReply(0);

        auto& [list, expiry] = listDb.at(key);
        if (isExpired(expiry))
        {
            listDb.erase(key);
            return createIntegerReply(0);
        }

        return createIntegerReply(static_cast<int>(list.size()));
    }


    
    CommandRegistrar<LLenCmd> registrar{};
};
