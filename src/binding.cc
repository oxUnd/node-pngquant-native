#include <node.h>
#include <node_buffer.h>
#include "PQCompress.h"

//
//void CreateObject(const FunctionCallbackInfo<Value>& args) {
//  Compress::Compress::NewInstance(args);
//}
//
//void InitAll(Local<Object> exports, Local<Object> module) {
//  Compress::Compress::Init(exports->GetIsolate());
//  NODE_SET_METHOD(module, "exports", CreateObject);
//}
//
//NODE_MODULE(addon, InitAll);

extern "C" {
napi_value Init(napi_env env, napi_value exports) {
  return PQCompress::Init(env, exports);
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init);
}