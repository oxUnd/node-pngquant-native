#include <png.h>
#include <stdlib.h>
#include <node_buffer.h>
#include "pngquant/pngquant.h"
#include "pngquant_native.h"

Persistent<FunctionTemplate> Pngquant::constructor;

void Pngquant::Init(Handle<Object> exports) {
    NanScope();

    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    Local<String> name = NanNew("Pngquant");

    // ObjectWrap uses the first internal field to store the wrapped pointer.
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    tpl->SetClassName(name);

    // Add all prototype methods, getters and setters here.
    NODE_SET_PROTOTYPE_METHOD(tpl, "compress", Compress);

    NanAssignPersistent(constructor, tpl);

    // This has to be last, otherwise the properties won't show up on the
    // object in JavaScript.
    exports->Set(name, tpl->GetFunction());
}

Pngquant::Pngquant():ObjectWrap() {
}

Handle<Value> Pngquant::New(const Arguments& args) {
    NanScope();

    if (!args.IsConstructCall()) {
        return NanThrowTypeError("Use the new operator to create instances of this object.");
    }

      // Creates a new instance object of this type and wraps it.
    Pngquant* obj = new Pngquant();

    obj->Wrap(args.This());

    NanReturnThis();
}


NAN_METHOD(Pngquant::Compress) {
    HandleScope scope;
    struct rwpng_data * out_buffer;
    struct rwpng_data * in_buffer;

    if (args.Length() != 3) {
        return NanThrowTypeError("Invalid argument, Need three arguments!");
    }

    if (!Buffer::HasInstance(args[0])) {
        return NanThrowTypeError("First argument must be a buffer.");
    }

    if (!args[2]->IsFunction()) {
        return NanThrowTypeError("Third argument must be a callback function.");
    }

    png_bytep in_stream = (png_bytep) Buffer::Data(args[0]->ToObject());
    unsigned in_length = Buffer::Length(args[0]->ToObject());
    Local<String> opt = args[1]->ToString();
    Local<Function> callback = Local<Function>::Cast(args[2]);

    char str_buffer[BUFFER_SIZE];
    memset(str_buffer, '\0', BUFFER_SIZE);

    opt->WriteUtf8(str_buffer);

    char *argv[32] = {""};

    char token[BUFFER_SIZE];
    memset(token, '\0', BUFFER_SIZE);

    int i = 0, argc = 0, k = 0, len = 0;
    while(str_buffer[i] != '\0') {
        if (argc >= 30) {
            break;
        }
        if (str_buffer[i] == ' ') {
            argc++;
            k = 0;
            len = strlen(token);
            argv[argc] = (char*) malloc(len + 1);
            memset(argv[argc], '\0', len + 1);
            memcpy(argv[argc], token, len + 1);
            //reset token
            memset(token, '\0', BUFFER_SIZE);
        } else {
            token[k++] = str_buffer[i];
        }
        i++;
    }

    if ((len = strlen(token)) > 0) {
        argc++;
        argv[argc] = (char*) malloc(len + 1);
        memset(argv[argc], '\0', len + 1);
        memcpy(argv[argc], token, len + 1);
        argc = argc + 1; //0 1 2
    }

    in_buffer = (struct rwpng_data *)malloc(sizeof(struct rwpng_data));

    if (in_buffer == NULL) {
        return NanThrowTypeError("malloc fail!");
    }

    out_buffer = (struct rwpng_data *)malloc(sizeof(struct rwpng_data));

    if (out_buffer == NULL) {
        return NanThrowTypeError("malloc fail!");
    }

    memset(out_buffer, '\0', sizeof(struct rwpng_data));

    in_buffer->png_data = in_stream;
    in_buffer->length = in_length;
    in_buffer->bytes_read = 0;

    int ret = pngquant_exec(in_buffer, out_buffer, argc, argv);
    if (ret != 0) {
        out_buffer->png_data = in_buffer->png_data;
        out_buffer->length = in_buffer->length;
        fprintf(stderr, " File: %s\n", argv[argc-1]);
    }

    Local<Object> buffer = NanNewBufferHandle((char *)out_buffer->png_data, out_buffer->length);
    free(in_buffer);
    free(out_buffer);

    NanReturnValue(buffer);
}

extern "C" {
    void RegisterModule(Handle<Object> exports) {
        Pngquant::Init(exports);
    }
}

NODE_MODULE(pngquant_native, RegisterModule);
