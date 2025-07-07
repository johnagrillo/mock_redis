#include "hiredis/hiredis.h"
#include "mock_redis.h"

auto main() -> int
{
    redisContext* redisContext = redisConnect("127.0.0.1", 6379);
    if (redisContext == nullptr || redisContext->err)
    {
        std::cerr << "Connection error: " << (redisContext ? redisContext->errstr : "can't allocate redis context")
                  << "\n";
        return 1;
    }

    //redisContext* redisContext = nullptr;

    // Sample set up for SMEMBERS
    // setDb["myset"] = {"one", "two", "three"};

    // Using the formatted command d
    //
    // ispatcher
    auto*r = redisCommandT(redisContext, "AUTH %s", "hunter2"); // AUTH hunter2 -> +OK
    std::cout << r << "\n";
    
    auto *v = redisCommand(redisContext, "SET %s %s", "mykey", "myvalue"); // SET mykey myvalue -> +OK
    std::cout << v << "\n";
    

    redisCommand(redisContext, "SET %s %d %s", "mykey", 10, "myvalue"); // SET mykey myvalue -> +OK
   
    redisCommand(redisContext, "GET %s", "mykey");               // GET mykey -> prints value
    redisCommand(redisContext, "PING");                          // PING -> +PONG
    redisCommand(redisContext, "GET %s", "nokey");               // GET nokey -> $-1

    // Test other commands with formatted string
    redisCommand(redisContext, "SMEMBERS %s", "myset");    // SMEMBERS myset -> list of members
    redisCommand(redisContext, "SET %s %s", "foo", "bar"); // SET foo bar -> +OK
    redisCommand(redisContext, "AUTH %s", "badpass");      // AUTH badpass -> -ERR invalid password
    redisCommand(redisContext, "PING");                    // PING -> -NOAUTH

    // --- TEST HSET/HGET ---
    redisCommand(redisContext, "HSET %s %s %s", "myhash", "field1", "hello"); // :1
    redisCommand(redisContext, "HGET %s %s", "myhash", "field1");             // hello
    redisCommand(redisContext, "HGET %s %s", "myhash", "nofield");            // $-1
    redisCommand(redisContext, "HGET %s %s", "nokey", "field1");              // $-1

    // --- TEST SADD ---
    redisCommand(redisContext, "SADD %s %s", "myset", "four"); // :1 (new element)
    redisCommand(redisContext, "SADD %s %s", "myset", "two");  // :0 (already exists)
    redisCommand(redisContext, "SMEMBERS %s", "myset");        // one, two, three, four

    // --- TEST SREM ---
    redisCommand(redisContext, "SREM %s %s", "myset", "two");      // :1 (removed)
    redisCommand(redisContext, "SREM %s %s", "myset", "notfound"); // :0 (not present)
    redisCommand(redisContext, "SMEMBERS %s", "myset");            // one, three, four

    auto& registry = CommandRegistry::get();

    std::cout << "Registered commands:\n";

    for (const auto& [format, info] : registry)
    {
        std::cout << format << "\n";
    }

    redisCommand(redisContext , "AUTH %s", "hunter2");
    // Set("key", "value");
    // Get("key");
    // Get("none");

    return 0;
}