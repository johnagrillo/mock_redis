#include "hiredis.h"
#include <cstddef>
#include <cstdlib>
#include <iostream>







/* Free a reply object */
void freeReplyObject(void* reply)
{
  std::cerr << "free " << reply << "\n";
  redisReply* r = (redisReply*)reply;
  size_t j;

  if (r == NULL)
    return;

  switch (r->type) {
  case REDIS_REPLY_INTEGER:
  case REDIS_REPLY_NIL:
  case REDIS_REPLY_BOOL:
    break; /* Nothing to free */
  case REDIS_REPLY_ARRAY:
  case REDIS_REPLY_MAP:
    //case REDIS_REPLY_ATTR:
  case REDIS_REPLY_SET:
  case REDIS_REPLY_PUSH:
    if (r->element != NULL) {
      for (j = 0; j < r->elements; j++)
        freeReplyObject(r->element[j]);
      free(r->element);
    }
    break;
  case REDIS_REPLY_ERROR:
  case REDIS_REPLY_STATUS:
  case REDIS_REPLY_STRING:
  case REDIS_REPLY_DOUBLE:
  case REDIS_REPLY_VERB:
  case REDIS_REPLY_BIGNUM:
    free(r->str);
    break;
  }
  free(r);
}

