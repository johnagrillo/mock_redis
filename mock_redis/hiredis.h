#pragma once

#define REDIS_REPLY_STRING     1
#define REDIS_REPLY_ARRAY      2
#define REDIS_REPLY_INTEGER    3
#define REDIS_REPLY_NIL        4
#define REDIS_REPLY_STATUS     5
#define REDIS_REPLY_ERROR      6
#define REDIS_REPLY_DOUBLE     7
#define REDIS_REPLY_BOOL       8  
#define REDIS_REPLY_MAP        9
#define REDIS_REPLY_SET       10
#define REDIS_REPLY_PUSH      11
#define REDIS_REPLY_BIGNUM    12
#define REDIS_REPLY_VERB      13
#define HIREDIS_MAJOR 1
#define HIREDIS_MINOR 3
#define HIREDIS_PATCH 0
#define HIREDIS_SONAME 1.3.0


using redisReplay = struct redisReply
{
  int type; /* REDIS_REPLY_* */
  long long integer; /* The integer when type is REDIS_REPLY_INTEGER */
  double dval; /* The double when type is REDIS_REPLY_DOUBLE */
  size_t len; /* Length of string */
  char* str; /* Used for REDIS_REPLY_ERROR, REDIS_REPLY_STRING
                REDIS_REPLY_VERB, REDIS_REPLY_DOUBLE (in additional to dval),
                and REDIS_REPLY_BIGNUM. */
  char vtype[4]; /* Used for REDIS_REPLY_VERB, contains the null
                    terminated 3 character content type, such as "txt". */
  size_t elements; /* number of elements, for REDIS_REPLY_ARRAY */
  struct redisReply** element; /* elements vector for REDIS_REPLY_ARRAY */
};

void freeReplyObject(void* reply);

