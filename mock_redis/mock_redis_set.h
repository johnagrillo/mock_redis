
#pragma once

#include "mock_redis.h"


// -------------------
// SADD Command
// -------------------
struct SAddTag : AutoRegister<SAddTag>
{
  static constexpr const char* tag = "SADD";
  static constexpr const char* format = "SADD %s %s";  // %s for key and member
  using ArgTypes = std::tuple<std::string, std::string>;
  
  static redisReply* call(const std::string& key, const std::string& member)
  {
    auto * reply = createRedisReply();

    if (!isAuth) {
      reply->type = REDIS_REPLY_ERROR;
      reply->str = (char*)"-NOAUTH Authentication required";
      reply->len = strlen(reply->str);
      return reply;
    }

    bool inserted = setDb[key].insert(member).second;
    reply->type = REDIS_REPLY_INTEGER;
    reply->integer = inserted ? 1 : 0;
    return reply;
  }
};


// -------------------
// SMEMBERS Command
// -------------------
struct SMembersTag : AutoRegister<SMembersTag>
{
  static constexpr const char* tag = "SMEMBERS";
  static constexpr const char* format = "SMEMBERS %s";  // %s for key
  using ArgTypes = std::tuple<std::string>; // key (name of the set)

  static CommandResult call(const std::string& key)
  {
    auto* reply = createRedisReply();
    
    if (!isAuth) {
      reply->type = REDIS_REPLY_ERROR;
      reply->str = (char*)"-NOAUTH Authentication required";
      reply->len = strlen(reply->str);
      return reply;
    }

    auto it = setDb.find(key);
    if (it == setDb.end()) {
      reply->type = REDIS_REPLY_ARRAY;
      reply->elements = 0;
      reply->element = nullptr;
    }
    else {
      reply->type = REDIS_REPLY_ARRAY;
      reply->elements = it->second.size();
      reply->element = (redisReply**)malloc(it->second.size() * sizeof(redisReply*));

      int i = 0;
      for (const auto& item : it->second) {
        reply->element[i] = (redisReply*)malloc(sizeof(redisReply));
        reply->element[i]->type = REDIS_REPLY_STRING;
        reply->element[i]->str = (char*)item.c_str();
        reply->element[i]->len = item.length();
        i++;
      }
    }

    return reply;
  }
};

// -------------------
// SREM Command
// -------------------
struct SRemTag: AutoRegister<SRemTag>
{
  static constexpr const char* tag = "SREM";
  static constexpr const char* format = "SREM %s %s";  // %s for key and member
  using ArgTypes = std::tuple<std::string, std::string>;

  static redisReply* call(const std::string& key, const std::string& member)
  {
    auto* reply = createRedisReply();

    if (!isAuth) {
      reply->type = REDIS_REPLY_ERROR;
      reply->str = (char*)"-NOAUTH Authentication required";
      reply->len = strlen(reply->str);
      return reply;
    }

    auto it = setDb.find(key);
    bool removed = false;
    if (it != setDb.end()) {
      removed = it->second.erase(member) > 0;
    }

    reply->type = REDIS_REPLY_INTEGER;
    reply->integer = removed ? 1 : 0;
    return reply;
  }
};
