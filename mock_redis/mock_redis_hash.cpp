// Hash Commands Implementation
#include "mock_redis.h"

static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> hashDb;


struct HSetTag: AutoRegister<HSetTag>
{
    static constexpr const char* tag = "HSET";
    static constexpr const char* format = "HSET %s %s %s";
    using ArgTypes = std::tuple<std::string, std::string, std::string>;

    static CommandResult call(const std::string& key, const std::string& field, const std::string& value)
    {
        if (!isAuth) return createAuthErrorReply();

        auto& fieldMap = hashDb[key];
        bool isNewField = fieldMap.find(field) == fieldMap.end();
        fieldMap[field] = value;

        return createIntegerReply(isNewField ? 1 : 0);
    }
};

struct HGetTag: AutoRegister<HGetTag>
{
    static constexpr const char* tag = "HGET";
    static constexpr const char* format = "HGET %s %s";
    using ArgTypes = std::tuple<std::string, std::string>;

    static CommandResult call(const std::string& key, const std::string& field)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!hashDb.contains(key)) return createNilReply();

        auto& fieldMap = hashDb.at(key);
        if (!fieldMap.contains(field)) return createNilReply();

        return createStringReply(fieldMap.at(field));
    }
};

struct HDelTag : AutoRegister<HDelTag>
{
    static constexpr const char* tag = "HDEL";
    static constexpr const char* format = "HDEL %s %s";
    using ArgTypes = std::tuple<std::string, std::string>;

    static CommandResult call(const std::string& key, const std::string& field)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!hashDb.contains(key)) return createIntegerReply(0);

        auto& fieldMap = hashDb.at(key);
        size_t removed = fieldMap.erase(field);

        if (fieldMap.empty()) hashDb.erase(key);

        return createIntegerReply(static_cast<int>(removed));
    }
};

struct HExistsTag:AutoRegister<HExistsTag>
{
    static constexpr const char* tag = "HEXISTS";
    static constexpr const char* format = "HEXISTS %s %s";
    using ArgTypes = std::tuple<std::string, std::string>;

    static CommandResult call(const std::string& key, const std::string& field)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!hashDb.contains(key)) return createIntegerReply(0);

        auto& fieldMap = hashDb.at(key);
        return createIntegerReply(fieldMap.contains(field) ? 1 : 0);
    }
};

struct HGetAllTag : AutoRegister<HGetAllTag>
{
    static constexpr const char* tag = "HGETALL";
    static constexpr const char* format = "HGETALL %s";
    using ArgTypes = std::tuple<std::string>;

    static CommandResult call(const std::string& key)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!hashDb.contains(key)) return createNilReply();

        auto& fieldMap = hashDb.at(key);
        if (fieldMap.empty()) return createNilReply();

        redisReply* reply = createArrayReply(fieldMap.size() * 2);

        size_t idx = 0;
        for (const auto& [field, val] : fieldMap)
        {
            reply->element[idx++] = createStringReply(field);
            reply->element[idx++] = createStringReply(val);
        }
        return reply;
    }
};

struct HKeysTag: AutoRegister<HSetTag>
{
    static constexpr const char* tag = "HKEYS";
    static constexpr const char* format = "HKEYS %s";
    using ArgTypes = std::tuple<std::string>;

    static CommandResult call(const std::string& key)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!hashDb.contains(key)) return createNilReply();

        auto& fieldMap = hashDb.at(key);
        if (fieldMap.empty()) return createNilReply();

        redisReply* reply = createArrayReply(fieldMap.size());
        size_t idx = 0;
        for (const auto& [field, _] : fieldMap)
        {
            reply->element[idx++] = createStringReply(field);
        }
        return reply;
    }
};

struct HValsTag: AutoRegister<HValsTag>
{
    static constexpr const char* tag = "HVALS";
    static constexpr const char* format = "HVALS %s";
    using ArgTypes = std::tuple<std::string>;

    static CommandResult call(const std::string& key)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!hashDb.contains(key)) return createNilReply();

        auto& fieldMap = hashDb.at(key);
        if (fieldMap.empty()) return createNilReply();

        redisReply* reply = createArrayReply(fieldMap.size());
        size_t idx = 0;
        for (const auto& [_, val] : fieldMap)
        {
            reply->element[idx++] = createStringReply(val);
        }
        return reply;
    }
};

struct HLenTag:AutoRegister<HLenTag>
{
    static constexpr const char* tag = "HLEN";
    static constexpr const char* format = "HLEN %s";
    using ArgTypes = std::tuple<std::string>;

    static CommandResult call(const std::string& key)
    {
        if (!isAuth) return createAuthErrorReply();

        if (!hashDb.contains(key)) return createIntegerReply(0);

        return createIntegerReply(static_cast<int>(hashDb.at(key).size()));
    }
};

struct HIncrByTag: AutoRegister<HIncrByTag>
{
    static constexpr const char* tag = "HINCRBY";
    static constexpr const char* format = "HINCRBY %s %s %d";
    using ArgTypes = std::tuple<std::string, std::string, int>;

    static CommandResult call(const std::string& key, const std::string& field, int increment)
    {
        if (!isAuth) return createAuthErrorReply();

        auto& fieldMap = hashDb[key];

        int current = 0;
        if (fieldMap.contains(field))
        {
            try
            {
                current = std::stoi(fieldMap[field]);
            }
            catch (...)
            {
                return createErrorReply("ERR hash value is not an integer");
            }
        }

        int newVal = current + increment;
        fieldMap[field] = std::to_string(newVal);
        return createIntegerReply(newVal);
    }
};
