#ifndef _PNG_PNG_QUANT_H_
#define _PNG_PNG_QUANT_H_

#include <node.h>

#define BUFFER_SIZE 1024

class Pngquant : public node::ObjectWrap {
public:
    static v8::Persistent<v8::FunctionTemplate> constructor;
    static void Init(v8::Handle<v8::Object> target);
    static v8::Handle<v8::Value> Compress(const v8::Arguments& args);

protected:
    Pngquant();
    static v8::Handle<v8::Value> New(const v8::Arguments& args);
};

#endif