#ifndef _PNG_PNG_QUANT_H_
#define _PNG_PNG_QUANT_H_

#include <nan.h>

#define BUFFER_SIZE 1024

class Pngquant : public node::ObjectWrap {
public:
    static v8::Persistent<v8::FunctionTemplate> constructor;
    static void Init(v8::Handle<v8::Object> exports);
    static NAN_METHOD(Compress);

protected:
    Pngquant();
    static NAN_METHOD(New);
};

#endif