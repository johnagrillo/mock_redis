
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <corecrt_io.h>
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "redis_reply.h"

class RedisClient
{
  public:
    RedisClient(std::string_view ip, int port) { connectToRedis(ip, port); }

    ~RedisClient()
    {
        if (sockfd >= 0) close(sockfd);
    }

    redis::Reply set(std::string_view key, std::string_view value)
    {
        auto response = sendCommand({"SET", key, value});
        return response;
    }

    redis::Reply get(std::string_view key)
    {
        responseBuffer.fill(0); // Clear previous data
        auto response = sendCommand({"GET", key});
        /*
        if (response.starts_with('$')) {
            size_t pos = response.find("\r\n");
            if (pos != std::string::npos) {
                // Return view into raw buffer (excluding \r\n and prefix)
                return std::string(response.data() + pos + 2, response.size() - pos - 4);
            }
        }
        */

        return response;
    }

  private:
    int sockfd = -1;
    std::array<char, 4096> responseBuffer{};

    void connectToRedis(std::string_view ip, int port)
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) throw std::runtime_error("ERROR: Socket creation failed");

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.data(), &serverAddr.sin_addr) <= 0)
            throw std::runtime_error("ERROR: Invalid IP address");

        if (connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0)
            throw std::runtime_error("ERROR: Connection failed");
    }

    std::string buildRESP(const std::vector<std::string_view>& parts)
    {
        std::ostringstream ss;
        ss << "*" << parts.size() << "\r\n";
        for (auto part : parts)
        {
            ss << "$" << part.size() << "\r\n" << part << "\r\n";
        }
        return ss.str(); // unavoidable single string alloc
    }

    redis::Reply sendCommand(const std::vector<std::string_view>& cmdParts)
    {
        std::string req = buildRESP(cmdParts);
        if (send(sockfd, req.data(), req.size(), 0) < 0) throw std::runtime_error("ERROR: Failed to send command");

        ssize_t received = recv(sockfd, responseBuffer.data(), responseBuffer.size() - 1, 0);
        if (received < 0) throw std::runtime_error("ERROR: Failed to receive response");

        responseBuffer[received] = '\0';
        std::cout << responseBuffer.data() << "\n";
        return parseRedisReply(std::string(responseBuffer.data(), received));
    }

    // This function parses the raw response into a redis::Reply
    redis::Reply parseRedisReply(const std::string& response)
    {
        if (response.empty())
        {
            throw std::runtime_error("ERROR: Empty response");
        }

        char type = response[0]; // The first byte tells us the response type

        redis::Reply reply;

        switch (type)
        {
        case '+': // Status Reply (e.g., "+OK")
            reply.type = redis::Type::Status;
            reply.value = std::string(response.begin() + 1, response.end());
            break;

        case '-': // Error Reply (e.g., "-ERR unknown command")
            reply.type = redis::Type::Error;
            reply.value = std::string(response.begin() + 1, response.end());
            break;

        case ':': // Integer Reply (e.g., ":12345")
            reply.type = redis::Type::Integer;
            reply.value = std::stoll(response.substr(1)); // Convert the integer part after ':'
            break;

        case '$': // Bulk String Reply (e.g., "$6\r\nfoobar\r\n")
        {
            size_t crlf = response.find("\r\n");
            long long len = std::stoll(response.substr(1, crlf - 1));
            reply.type = redis::Type::String;

            if (len == -1)
            {
                reply.value = std::monostate{}; // Null bulk string
            }
            else
            {
                size_t start = crlf + 2;
                reply.value = response.substr(start, len);
            }
        }
        break;

        case '*': // Array Reply (e.g., "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n")
        {
            size_t numElements = std::stoll(response.substr(1, response.find('\r') - 1)); // Get the number of elements
            size_t start = response.find("\r\n") + 2;                                     // Skip past "*2\r\n"
            std::vector<redis::Reply> arrayElements;

            for (size_t i = 0; i < numElements; ++i)
            {
                size_t nextElementStart = response.find("\r\n", start) + 2;
                size_t nextElementEnd = response.find("\r\n", nextElementStart);
                std::string element = response.substr(nextElementStart, nextElementEnd - nextElementStart);
                arrayElements.push_back(parseRedisReply(element));
                start = nextElementEnd + 2;
            }
            reply.type = redis::Type::Array;
            reply.value = redis::Array(arrayElements);
        }
        break;

        default: throw std::runtime_error("ERROR: Unsupported Redis reply type");
        }

        return reply;
    }
};
int main()
{
    try
    {
        RedisClient redis("127.0.0.1", 6379);

        std::cout << "set" << "\n";
        auto reply = redis.set("foo", "bar");
        redis::Reply::printReply(reply);

        std::cout << "get foo" << "\n";

        reply = redis.get("foo");
        redis::Reply::printReply(reply);

        std::cout << "get hello" << "\n";

        reply = redis.get("hrllo");
        redis::Reply::printReply(reply);

        // std::cout << "GET foo → " << value << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
    }

    return 0;
}
