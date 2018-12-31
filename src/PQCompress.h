#ifndef _H_OBJ_WRAP_H_
#define _H_OBJ_WRAP_H_

#include <png.h>
#include <node_api.h>

class PQCompress {
public:
  static napi_value Init(napi_env env, napi_value exports);
  static napi_value NewInstance(napi_env env, napi_callback_info info);
  static napi_value Invoke(napi_env env, napi_callback_info info);
  static void Destructor(napi_env env, void* nativeObject, void* finalize_hint);

private:
  explicit PQCompress(char *buf,  const unsigned length, const char * args);
  static napi_value New(napi_env env, napi_callback_info info);

  ~PQCompress();
  static int ParseArgs(const char *args, int n, char *argv[]);

  static napi_ref constructor;
  napi_env env_;
  napi_ref wrapper_;

  char * _buf;
  const char * _args;
  unsigned int _buf_length;
};


#endif