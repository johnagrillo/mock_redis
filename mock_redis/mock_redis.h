// mock_redis.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>

// TODO: Reference additional headers your program requires here.
redisReply* createRedisReply();


// Alias for unique_ptr with custom deleter
using RedisReplyPtr = std::unique_ptr<redisReply, decltype(&freeReplyObject)>;

redisReply* createStatusReply(const char* status);
redisReply* createOkStatusReply();
redisReply* createErrorReply(const char* error);
redisReply* createAuthErrorReply();
redisReply* createNilReply();
redisReply* createStringReply(const std::string& s);
redisReply* createIntegerReply(int value);
redisReply* createArrayReply(size_t count);
