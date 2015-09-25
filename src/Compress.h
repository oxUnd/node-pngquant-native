#ifndef _H_OBJ_WRAP_H_
#define _H_OBJ_WRAP_H_

#include <node.h>
#include <node_object_wrap.h>

extern "C" {
    #include <png.h>
}

namespace Compress {
  class Compress: public node::ObjectWrap {
  public:
    static v8::Persistent<v8::Function> constructor;

    static void Init(v8::Isolate* isolate);
    static void NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void Invoke(const v8::FunctionCallbackInfo<v8::Value>& args);

  private:
    explicit Compress(char *buf, const int buf_length, const char * args);
    ~Compress();
    static int ParseArgs(const char *args, int n, char *argv[]);
    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    char * _buf;
    const char * _args;
    unsigned int _buf_length;
  };
}

#endif