#pragma once

// Utility for std::visit
template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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

    // Pretty print with nesting
    static void printReply(const Reply& reply, int indent = 0)
    {
        std::string indentStr(indent, ' ');
        std::visit(overloaded{[&](const std::string& s) { std::cout << indentStr << "\"" << s << "\"\n"; },
                              [&](long long i) { std::cout << indentStr << i << "\n"; },
                              [&](double d) { std::cout << indentStr << d << "\n"; },
                              [&](bool b) { std::cout << indentStr << (b ? "true" : "false") << "\n"; },
                              [&](const std::monostate&) { std::cout << indentStr << "(nil)\n"; },
                              [&](const Array& arr)
                              {
                                  std::cout << indentStr << "[\n";
                                  for (const auto& item : arr.data)
                                  {
                                      printReply(item, indent + 2);
                                  }
                                  std::cout << indentStr << "]\n";
                              },
                              [&](const Set& set)
                              {
                                  std::cout << indentStr << "{set:\n";
                                  for (const auto& item : set.data)
                                  {
                                      printReply(item, indent + 2);
                                  }
                                  std::cout << indentStr << "}\n";
                              },
                              [&](const Push& push)
                              {
                                  std::cout << indentStr << "{push:\n";
                                  for (const auto& item : push.data)
                                  {
                                      printReply(item, indent + 2);
                                  }
                                  std::cout << indentStr << "}\n";
                              },
                              [&](const Map& map)
                              {
                                  std::cout << indentStr << "{\n";
                                  for (const auto& [key, val] : map.data)
                                  {
                                      std::cout << indentStr << "  ";
                                      printReply(key, 0);
                                      std::cout << indentStr << "  => ";
                                      printReply(val, 0);
                                  }
                                  std::cout << indentStr << "}\n";
                              },
                              [](const auto&) { std::cout << "(unknown type)\n"; }},
                   reply.value);
    }
};

} // namespace redis
