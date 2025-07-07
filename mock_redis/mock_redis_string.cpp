#include "mock_redis.h"

static std::unordered_map<std::string, std::pair<std::string, std::chrono::time_point<std::chrono::system_clock>>>
    strDb;

// Helper function to check expiration
bool isExpired(const std::chrono::time_point<std::chrono::system_clock>& expiryTime)
{
    return std::chrono::system_clock::now() > expiryTime;
}

struct SetBinaryCmd : AutoRegister<SetBinaryCmd>
{
    static constexpr const char* tag = "SETB";                      // distinguish from regular SET
    static constexpr const char* format = "SET %s %b";              // format string
    using ArgTypes = std::tuple<std::string, BinaryValue>;          // tuple of expected argument types

    static CommandResult call(const std::string& key, const BinaryValue& binVal)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        strDb[key] = {binVal.data, std::chrono::time_point<std::chrono::system_clock>::max()};

        return createOkStatusReply();
    }

};


struct SetExBinaryCmd : AutoRegister<SetExBinaryCmd>
{
    static constexpr const char* tag = "SETEXB";
    static constexpr const char* format = "SETEX %s %d %b"; // key, seconds, binary
    using ArgTypes = std::tuple<std::string, int, BinaryValue>;

    static CommandResult call(const std::string& key, int seconds, const BinaryValue& binVal)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
        strDb[key] = {binVal.data, expiry};

        return createOkStatusReply();
    }
};

// -------------------
// Exists Command
// -------------------

struct ExistsCmd: AutoRegister<ExistsCmd>
{
    static constexpr const char* tag = "EXISTS";
    static constexpr const char* format = "EXISTS %s"; // key
    using ArgTypes = std::tuple<std::string>;

    static CommandResult call(const std::string& key)
    {

        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        if (!strDb.contains(key))
        {
            return createIntegerReply(0);
        }

        const auto& [value, expiry] = strDb.at(key);
        if (isExpired(expiry))
        {
            return createIntegerReply(0);
        }

        return createIntegerReply(1);
    }
};

// -------------------
// Expire Command
// -------------------

struct ExpireCmd: AutoRegister<ExpireCmd>
{
    static constexpr const char* tag = "EXPIRE";
    static constexpr const char* format = "EXPIRE %s %d"; // key, seconds
    using ArgTypes = std::tuple<std::string, int>;

    static CommandResult call(const std::string& key, int seconds)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        if (!strDb.contains(key))
        {
            return createIntegerReply(0); // not found
        }

        auto& [value, expiry] = strDb.at(key);

        if (isExpired(expiry))
        {
            return createIntegerReply(0); // already expired
        }

        expiry = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
        return createIntegerReply(1); // updated
    }
};

// -------------------
// Get Command
// -------------------
struct GetCmd: AutoRegister<GetCmd>
{
    static constexpr const char* tag = "GET";
    static constexpr const char* format = "GET %s"; // %s for key
    using ArgTypes = std::tuple<std::string>;       // key

    static CommandResult call(const std::string& key)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        if (!strDb.contains(key))
        {
            return createNilReply();
        }

        auto& [value, expiry] = strDb.at(key);

        if (isExpired(expiry))
        {
            strDb.erase(key); // Remove expired key
            return createNilReply();
        }

        return createStringReply(value);
    }
};

// -------------------
// Set Command
// -------------------

struct SetCmd: AutoRegister<SetCmd>
{
    static constexpr const char* tag = "SET";
    static constexpr const char* format = "SET %s %s";
    using ArgTypes = std::tuple<std::string, std::string>;

    static CommandResult call(const std::string& key, const std::string& val)
    {

        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        // No expiration: store with epoch time (never expires)
        strDb[key] = {val, std::chrono::time_point<std::chrono::system_clock>::max()};

        return createOkStatusReply();
    }
};
// -------------------
// SetEx Command
// -------------------

struct SetExCmd: AutoRegister<SetExCmd>
{
    static constexpr const char* tag = "SETEX";
    static constexpr const char* format = "SETEX %s %d %s"; // key, seconds, value
    using ArgTypes = std::tuple<std::string, int, std::string>;

    static CommandResult call(const std::string& key, int seconds, const std::string& val)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
        strDb[key] = {val, expiry};

        return createOkStatusReply();
    }
};

// -------------------
// TTL Command
// -------------------

struct TTLCmd: AutoRegister<TTLCmd>
{
    static constexpr const char* tag = "TTL";
    static constexpr const char* format = "TTL %s"; // single string argument
    using ArgTypes = std::tuple<std::string>;

    static CommandResult call(const std::string& key)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        if (!strDb.contains(key))
        {
            return createIntegerReply(-2); // Key not found
        }

        auto& [value, expiry] = strDb.at(key);

        if (expiry == std::chrono::time_point<std::chrono::system_clock>::max())
        {
            return createIntegerReply(-1); // No expiration
        }

        auto now = std::chrono::system_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::seconds>(expiry - now).count();

        if (remaining <= 0)
        {
            strDb.erase(key);              // Clean up expired key
            return createIntegerReply(-2); // Expired = not found
        }

        return createIntegerReply(static_cast<int>(remaining));
    }
};
