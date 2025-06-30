
#include "mock_redis.h"

// -------------------
// Auth Command
// -------------------



struct AuthCmd : AutoRegister<AuthCmd>
{
    static constexpr const char* tag = "AUTH";
    static constexpr const char* format = "AUTH %s"; // %s will be replaced by the password
    using ArgTypes = std::tuple<std::string>;        // password

    CommandResult operator() (const std::string& password)
    { 
      AuthCmd::call(password);
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
