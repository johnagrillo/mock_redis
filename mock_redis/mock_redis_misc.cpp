
#include "mock_redis.h"

// -------------------
// Auth Command
// -------------------

#include <string>
#include <unordered_map>
#include <unordered_set>

// Global map of channel to subscriber identifiers (simplified)
static std::unordered_map<std::string, std::unordered_set<std::string>> channelSubscribers;

struct AuthCmd : AutoRegister<AuthCmd>
{
    static constexpr const char* tag = "AUTH";
    static constexpr const char* format = "AUTH %s"; // %s will be replaced by the password
    using ArgTypes = std::tuple<std::string>;        // password

    CommandResult operator() (const std::string& password)
    { 
      return AuthCmd::call(password);
    }

    static CommandResult call(const std::string& password)
    {
        if (password == "hunter2")
        {
            isAuth = true;
            return createOkStatusReply();
        }
        isAuth = false;
        return createErrorReply("-ERR invalid password");
    }
};

// -------------------
// Ping Command
// -------------------
struct PingCmd : AutoRegister<AuthCmd>
{
    static constexpr const char* tag = "PING";
    static constexpr const char* format = "PING"; // No arguments, just "PING"
    using ArgTypes = std::tuple<>;                // No arguments for PING

    static CommandResult call()
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }
        return createStringReply("+PONG");
    }
};



// ---------
//  PUBLISH CMD
// ---------

struct PublishCmd
{
    static constexpr const char* tag = "PUBLISH";
    static constexpr const char* format = "PUBLISH %s %s";
    using ArgTypes = std::tuple<std::string, std::string>; // channel, message

    static CommandResult call(const std::string& channel, const std::string& message)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        // Find subscribers to the channel
        auto it = channelSubscribers.find(channel);
        if (it == channelSubscribers.end())
        {
            // No subscribers
            return createIntegerReply(0);
        }

        const auto& subscribers = it->second;

        // For this mock, we just count subscribers.
        // In a real Redis, you'd deliver messages to connected clients here.
        int deliveredCount = static_cast<int>(subscribers.size());

        // Log publishing (optional)
        std::cout << "[Publish] Channel: " << channel << ", Message: " << message << ", Subscribers: " << deliveredCount
                  << std::endl;

        return createIntegerReply(deliveredCount);
    }

    static inline CommandRegistrar<PublishCmd> registrar{};
};



// ---------
//  SUBSCRIBE CMD
// ---------

struct SubscribeCmd
{
    static constexpr const char* tag = "SUBSCRIBE";
    static constexpr const char* format = "SUBSCRIBE %s %s"; // channel subscriberId
    using ArgTypes = std::tuple<std::string, std::string>;   // channel, subscriber ID

    static CommandResult call(const std::string& channel, const std::string& subscriberId)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        channelSubscribers[channel].insert(subscriberId);

        // In real Redis, SUBSCRIBE returns messages about subscription; here just OK
        return createOkStatusReply();
    }

    static inline CommandRegistrar<SubscribeCmd> registrar{};
};


// ---------
//  UNSUBSCRIBE CMD
// ---------

struct UnsubscribeCmd
{
    static constexpr const char* tag = "UNSUBSCRIBE";
    static constexpr const char* format = "UNSUBSCRIBE %s %s"; // channel subscriberId
    using ArgTypes = std::tuple<std::string, std::string>;     // channel, subscriber ID

    static CommandResult call(const std::string& channel, const std::string& subscriberId)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        auto it = channelSubscribers.find(channel);
        if (it != channelSubscribers.end())
        {
            it->second.erase(subscriberId);
            if (it->second.empty())
            {
                channelSubscribers.erase(it);
            }
        }

        return createOkStatusReply();
    }

    static inline CommandRegistrar<UnsubscribeCmd> registrar{};
};


// ---------
//  LISTSUB CMD
// ---------

struct ListSubCmd
{
    static constexpr const char* tag = "LISTSUB";
    static constexpr const char* format = "LISTSUB %s"; // channel
    using ArgTypes = std::tuple<std::string>;           // channel

    static CommandResult call(const std::string& channel)
    {
        if (!isAuth)
        {
            return createAuthErrorReply();
        }

        auto it = channelSubscribers.find(channel);
        if (it == channelSubscribers.end())
        {
            // No subscribers, return empty array
            return createArrayReply(0);
        }

        const auto& subs = it->second;
        redisReply* reply = createArrayReply(subs.size());

        for (size_t i = 0; i < subs.size(); ++i)
        {
            // NOTE: redisReply->element is an array of pointers
            reply->element[i] = createStringReply(*std::next(subs.begin(), i));
        }

        return reply;
    }

    static inline CommandRegistrar<ListSubCmd> registrar{};
};