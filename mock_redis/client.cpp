
#include <array>
#include <cstring>
#include <exception>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#ifdef _WIN32
#include <corecrt_io.h>
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <algorithm>

#include "redis_reply.h"

class RedisClient
{
  public:
    RedisClient(std::string_view ip, int port) { connectToRedis(ip, port); }

    ~RedisClient()
    {
        if (sockfd >= 0)
        {
            close(sockfd);
        }
    }

    auto set(std::string_view key, std::string_view value) -> redis::Reply { return sendCommand({"SET", key, value}); }
    auto get(std::string_view key) -> redis::Reply { return sendCommand({"GET", key}); }
    auto incr(std::string_view key) -> redis::Reply { return sendCommand({"INCR", key}); }
    auto decr(std::string_view key) -> redis::Reply { return sendCommand({"DECR", key}); }
    auto incrBy(std::string_view key, long long increment) -> redis::Reply
    {
        return sendCommand({"INCRBY", key, std::to_string(increment)});
    }
    auto decrBy(std::string_view key, long long decrement) -> redis::Reply
    {
        return sendCommand({"DECRBY", key, std::to_string(decrement)});
    }
    auto append(std::string_view key, std::string_view value) -> redis::Reply
    {
        return sendCommand({"APPEND", key, value});
    }
    auto strlen(std::string_view key) -> redis::Reply { return sendCommand({"STRLEN", key}); }
    auto getSet(std::string_view key, std::string_view value) -> redis::Reply
    {
        return sendCommand({"GETSET", key, value});
    }
    auto mset(const std::vector<std::pair<std::string, std::string>>& kvs) -> redis::Reply
    {
        std::vector<std::string_view> parts;
        parts.reserve(1 + (kvs.size() * 2));
        parts.emplace_back("MSET");
        for (auto& [k, v] : kvs)
        {
            parts.emplace_back(k);
            parts.emplace_back(v);
        }
        return sendCommand(parts);
    }

    // MGET key1 [key2 ...]
    auto mget(const std::vector<std::string>& keys) -> redis::Reply
    {
        std::vector<std::string_view> parts;
        parts.reserve(1 + keys.size());
        parts.emplace_back("MGET");
        for (auto& k : keys)
            parts.emplace_back(k);
        return sendCommand(parts);
    }

    auto hset(std::string_view key, std::string_view field, std::string_view value) -> redis::Reply
    {
        return sendCommand({"HSET", key, field, value});
    }

    auto hget(std::string_view key, std::string_view field) -> redis::Reply
    {
        return sendCommand({"HGET", key, field});
    }

    auto hgetall(std::string_view key) -> redis::Reply { return sendCommand({"HGETALL", key}); }

  private:
    int sockfd = -1;
    std::array<char, 4096> responseBuffer{};

    void connectToRedis(std::string_view ip, int port)
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            throw std::runtime_error("ERROR: Socket creation failed");
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.data(), &serverAddr.sin_addr) <= 0)
        {
            throw std::runtime_error("ERROR: Invalid IP address");
        }

        if (connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0)
        {
            throw std::runtime_error("ERROR: Connection failed");
        }
    }

    static auto buildRESP(const std::vector<std::string_view>& parts) -> std::string
    {
        std::ostringstream ss;
        ss << "*" << parts.size() << "\r\n";
        for (auto part : parts)
        {
            ss << "$" << part.size() << "\r\n" << part << "\r\n";
        }
        return ss.str(); // unavoidable single string alloc
    }

    auto sendCommand(const std::vector<std::string_view>& cmdParts) -> redis::Reply const
    {

        std::cout << "----------------- Command parts: ";
        std::ranges::for_each(cmdParts, [](std::string_view part) { std::cout << "[" << part << "] "; });
        std::cout << "\n";
        std::string req = buildRESP(cmdParts);

        std::cout << req << "\n";
        if (send(sockfd, req.data(), req.size(), 0) < 0)
        {
            throw std::runtime_error("ERROR: Failed to send command");
        }

        ssize_t received = recv(sockfd, responseBuffer.data(), responseBuffer.size() - 1, 0);
        if (received < 0)
        {
            throw std::runtime_error("ERROR: Failed to receive response");
        }

        responseBuffer[received] = '\0';
        std::cout << responseBuffer.data() << "\n";
        return parseRedisReply(std::string(responseBuffer.data(), received));
    }

    static auto parseSingleReply(const std::string& response, size_t offset) -> std::pair<redis::Reply, size_t>
    {
        if (offset >= response.size())
        {
            throw std::runtime_error("ERROR: Offset past response size");
        }

        char const type = response[offset];
        size_t const curr = offset + 1;
        redis::Reply reply;

        switch (type)
        {
        case '+':
        { // Status
            size_t const crlf = response.find("\r\n", curr);
            reply.type = redis::Type::Status;
            reply.value = response.substr(curr, crlf - curr);
            return {reply, crlf + 2 - offset};
        }
        case '-':
        { // Error
            size_t const crlf = response.find("\r\n", curr);
            reply.type = redis::Type::Error;
            reply.value = response.substr(curr, crlf - curr);
            return {reply, crlf + 2 - offset};
        }
        case ':':
        { // Integer
            size_t const crlf = response.find("\r\n", curr);
            reply.type = redis::Type::Integer;
            reply.value = std::stoll(response.substr(curr, crlf - curr));
            return {reply, crlf + 2 - offset};
        }
        case '$':
        { // Bulk string
            size_t const crlf = response.find("\r\n", curr);
            long long const len = std::stoll(response.substr(curr, crlf - curr));
            reply.type = redis::Type::String;
            if (len == -1)
            {
                reply.value = std::monostate{};
                return {reply, crlf + 2 - offset};
            }

            size_t start = crlf + 2;
            reply.value = response.substr(start, len);
            return {reply, start + len + 2 - offset}; // +2 for \r\n
        }
        case '*':
        { // Array
            size_t const crlf = response.find("\r\n", curr);
            long long const count = std::stoll(response.substr(curr, crlf - curr));
            std::vector<redis::Reply> elements;
            size_t next = crlf + 2;
            for (int i = 0; i < count; ++i)
            {
                auto [elem, used] = parseSingleReply(response, next);
                elements.push_back(std::move(elem));
                next += used;
            }
            reply.type = redis::Type::Array;
            reply.value = redis::Array{std::move(elements)};
            return {reply, next - offset};
        }
        default: throw std::runtime_error("ERROR: Unsupported Redis reply type");
        }
    }

    static auto parseRedisReply(const std::string& response) -> redis::Reply
    {
        auto [reply, used] = parseSingleReply(response, 0);
        return reply;
    }
};

auto main() -> int
{
    try
    {
        RedisClient redis("127.0.0.1", 6379);

        auto reply = redis.set("foo", "bar");
        redis::Reply::printReply(reply);

        reply = redis.get("foo");
        redis::Reply::printReply(reply);

        reply = redis.get("hrllo");
        redis::Reply::printReply(reply);

        reply = redis.set("count", "10");
        redis::Reply::printReply(reply);

        reply = redis.incr("count"); // should be 11
        redis::Reply::printReply(reply);

        reply = redis.incrBy("count", 5); // should be 16
        redis::Reply::printReply(reply);

        reply = redis.append("count", "XYZ");
        redis::Reply::printReply(reply); // returns new length

        reply = redis.strlen("count");
        redis::Reply::printReply(reply);

        reply = redis.getSet("greeting", "hello");
        // prints old value or nil
        redis::Reply::printReply(reply);

        reply = redis.mset({{"a", "1"}, {"b", "2"}});
        redis::Reply::printReply(reply);

        reply = redis.mget({"a", "b", "c"});
        redis::Reply::printReply(reply);

        std::cout << "\nHSET myhash field1 value1\n";
        auto hsetReply = redis.hset("myhash", "field1", "value1");
        redis::Reply::printReply(hsetReply);

        std::cout << "\nHGET myhash field1\n";
        auto hgetReply = redis.hget("myhash", "field1");
        redis::Reply::printReply(hgetReply);

        std::cout << "\nHGETALL myhash\n";
        auto hgetallReply = redis.hgetall("myhash");
        redis::Reply::printReply(hgetallReply);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
    }

    return 0;
}
