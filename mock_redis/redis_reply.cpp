#include "redis_reply.h"

#include <cstring>
#include <memory>

#include "hiredis/hiredis.h"

namespace
{
redisReply* allocRawReply()
{
    auto* r = static_cast<redisReply*>(std::malloc(sizeof(redisReply)));
    if (r)
    {
        std::memset(r, 0, sizeof(redisReply));
    }
    return r;
}

// Internal recursive helper
redisReply* convertToRawReply(const redis::Reply& reply)
{
    redisReply* r = allocRawReply();
    if (!r) return nullptr;

    r->type = static_cast<int>(reply.type);

    switch (reply.type)
    {
    case redis::Type::String:
    case redis::Type::Status:
    case redis::Type::Error:
    case redis::Type::BigNum:
    case redis::Type::Verb:
    {
        const auto& str = std::get<std::string>(reply.value);
        r->len = str.size();
        r->str = static_cast<char*>(std::malloc(r->len + 1));
        std::memcpy(r->str, str.data(), r->len);
        r->str[r->len] = '\0';

        if (reply.type == redis::Type::Verb && reply.vtype.has_value())
        {
            std::strncpy(r->vtype, reply.vtype->c_str(), sizeof(r->vtype) - 1);
            r->vtype[sizeof(r->vtype) - 1] = '\0';
        }
        break;
    }

    case redis::Type::Integer: r->integer = std::get<long long>(reply.value); break;

    case redis::Type::Double: r->dval = std::get<double>(reply.value); break;

    case redis::Type::Bool: r->integer = std::get<bool>(reply.value) ? 1 : 0; break;

    case redis::Type::Nil:
        // Nothing to do
        break;

    case redis::Type::Array:
    case redis::Type::Set:
    case redis::Type::Push:
    {
        const auto& vec = std::get<redis::Array>(reply.value);
        r->len = vec.data.size();
        r->element = static_cast<redisReply**>(std::malloc(sizeof(redisReply*) * r->len));
        for (size_t i = 0; i < r->len; ++i)
        {
            r->element[i] = convertToRawReply(vec.data[i]);
        }
        break;
    }

    case redis::Type::Map:
    {
        const auto& map = std::get<redis::Map>(reply.value);
        r->len = map.data.size() * 2;
        r->element = static_cast<redisReply**>(std::malloc(sizeof(redisReply*) * r->len));
        size_t i = 0;
        for (const auto& [key, val] : map.data)
        {
            r->element[i++] = convertToRawReply(key);
            r->element[i++] = convertToRawReply(val);
        }
        break;
    }
    }

    return r;
}

} // namespace

namespace redis
{



} // namespace redis
