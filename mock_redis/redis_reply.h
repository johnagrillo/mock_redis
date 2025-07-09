#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "hiredis/hiredis.h"

namespace redis
{

// Forward declare Reply
struct Reply;

// Custom deleter for hiredis reply objects
struct RedisReplyDeleter
{
    void operator()(redisReply* reply) const
    {
        if (reply)
        {
            freeReplyObject(reply);
        }
    }
};

using UniqueRedisReply = std::unique_ptr<redisReply, RedisReplyDeleter>;

UniqueRedisReply ToRedisReply(const Reply& reply);

// Enum for Redis reply types
enum class Type
{
    String = 1,
    Array = 2,
    Integer = 3,
    Nil = 4,
    Status = 5,
    Error = 6,
    Double = 7,
    Bool = 8,
    Map = 9,
    Set = 10,
    Push = 11,
    BigNum = 12,
    Verb = 13
};

// Generic container holder template with a Tag type to make unique types

template <typename T, typename Tag> struct ContainerHolder
{
    T data;

    // Allow iteration
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    // Allow indexing
    const auto& operator[](size_t i) const { return data[i]; }

    // Allow size() and empty()
    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }

    // Add this equality operator:
    bool operator==(const ContainerHolder& other) const { return data == other.data; }
    bool operator<(const ContainerHolder& other) const { return data < other.data; }
};


// Tags to distinguish container types that use the same underlying container
struct ArrayTag
{
};
struct SetTag
{
};
struct PushTag
{
};
struct MapTag
{
};

// Type aliases with unique tags
using Array = ContainerHolder<std::vector<Reply>, ArrayTag>;
using Set = ContainerHolder<std::vector<Reply>, SetTag>;
using Push = ContainerHolder<std::vector<Reply>, PushTag>;
using Map = ContainerHolder<std::map<Reply, Reply>, MapTag>;

// The variant type that can hold any kind of Redis reply value
using ReplyValue = std::variant<std::monostate, // For Nil reply
                                std::string,    // String, Status, Error, BigNum, Verb (vtype separate)
                                long long,      // Integer
                                double,         // Double
                                bool,           // Bool
                                Array,          // Array
                                Map,            // Map
                                Set,            // Set
                                Push            // Push
                               
                                >;

// Main Reply structure
struct Reply
{
    Type type;
    ReplyValue value;
    std::optional<std::string> vtype; // Only for Verb replies, e.g. "txt"

    Reply() = default;

    Reply(Type t, ReplyValue v, std::optional<std::string> vt = std::nullopt)
        : type(t),
          value(std::move(v)),
          vtype(std::move(vt))
    {
    }

    bool operator==(const Reply& other) const
    {
        return std::tie(type, value, vtype) == std::tie(other.type, other.value, other.vtype);
    }
 
    bool operator<(const Reply& other) const
    {
        return std::tie(type, value, vtype) < std::tie(other.type, other.value, other.vtype);
    }
    static void printReply(const redis::Reply& reply)
    {
        using std::cout;
        using std::endl;

        switch (reply.type)
        {
        case redis::Type::String:
        case redis::Type::Status:
        case redis::Type::Error:
            if (std::holds_alternative<std::monostate>(reply.value))
            {
                cout << "String/Status/Error: (nil)" << endl;
            }
            else
            {
                cout << "String/Status/Error: " << std::get<std::string>(reply.value) << endl;
            }
            break;

        case redis::Type::Integer: cout << "Integer: " << std::get<long long>(reply.value) << endl; break;

        case redis::Type::Double: cout << "Double: " << std::get<double>(reply.value) << endl; break;

        //case redis::Type::Boolean:
        //    cout << "Boolean: " << (std::get<bool>(reply.value) ? "true" : "false") << endl;
        //    break;

        case redis::Type::Array:
        {
            const redis::Array& arr = std::get<redis::Array>(reply.value);
            if (arr.empty())
            {
                cout << "Array: []" << endl;
            }
            else
            {
                cout << "Array:" << endl;
                for (size_t i = 0; i < arr.size(); ++i)
                {
                    cout << "  [" << i << "]: ";
                    printReply(arr[i]); // recursive print
                }
            }
            break;
        }

        case redis::Type::Map:
        {
            const redis::Map& map = std::get<redis::Map>(reply.value);
            cout << "Map:" << endl;
            for (const auto& [key, val] : map)
            {
                cout << "  Key: ";
                printReply(key);
                cout << "  Value: ";
                printReply(val);
            }
            break;
        }

        case redis::Type::Set:
        {
            const redis::Set& set = std::get<redis::Set>(reply.value);
            cout << "Set:" << endl;
            for (const auto& elem : set)
            {
                cout << "  ";
                printReply(elem);
            }
            break;
        }

        case redis::Type::Push:
        {
            const redis::Push& push = std::get<redis::Push>(reply.value);
            cout << "Push:" << endl;
            for (size_t i = 0; i < push.size(); ++i)
            {
                cout << "  [" << i << "]: ";
                printReply(push[i]);
            }
            break;
        }

        case redis::Type::Nil:
        default: cout << "(nil)" << endl; break;
        }
    }
};

} // namespace redis
