#include <node.h>
#include <node_buffer.h>
#include "Compress.h"

namespace x {

  using v8::FunctionCallbackInfo;
  using v8::Local;
  using v8::Object;
  using v8::Value;
  using v8::Isolate;
  using v8::MaybeLocal;

  void CreateObject(const FunctionCallbackInfo<Value>& args) {
    Compress::Compress::NewInstance(args);
  }

  void InitAll(Local<Object> exports, Local<Object> module) {
    Compress::Compress::Init(exports->GetIsolate());
    NODE_SET_METHOD(module, "exports", CreateObject);
  }

  NODE_MODULE(addon, InitAll);
}