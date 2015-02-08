extern "C" {
    #include "libpng/png.h"
}

#include <stdlib.h>
#include <string.h>
#include "nan.h"
#include "pngquant/pngquant.h"
#include "pngquant_native.h"

int parseArgv(char *args, char *argv[], int p_size) {
    int i, len = (int)strlen(args), k = 0, argc = 0, is_new = 0;
    char *buf = (char *) malloc (p_size);
    memset(buf, '\0', p_size);
    
    for (i = 0; i < len + 1; i++) {
        if (args[i] != ' ' && i < len) {
            if (k < p_size) {
                buf[k++] = args[i];
                is_new = 1;
            }
        } else {
            if (is_new == 1) {
                strncpy(argv[argc], buf, k);
                memset(buf, '\0', k); //reset
                k = 0; //reset
                argc++;
                is_new = 0;
            }
        }
    }
    
    free(buf);
    
    return argc;
}

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> Pngquant::constructor;

void Pngquant::Init(Handle<Object> exports) {
    NanScope();

    Local<FunctionTemplate> tpl = NanNew<FunctionTemplate>(New);
    Local<String> name = NanNew("Pngquant");

    // constructor = Persistent<FunctionTemplate>::New(tpl);
    // ObjectWrap uses the first internal field to store the wrapped pointer.
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    tpl->SetClassName(name);

    // Add all prototype methods, getters and setters here.
    NODE_SET_PROTOTYPE_METHOD(tpl, "compress", Compress);

    // This has to be last, otherwise the properties won't show up on the
    // object in JavaScript.
    exports->Set(name, tpl->GetFunction());
}

Pngquant::Pngquant():ObjectWrap() {
}

NAN_METHOD(Pngquant::New) {
    NanScope();

    if (!args.IsConstructCall()) {
        return NanThrowTypeError(
            "Use the new operator to create instances of this object."
        );
    }

      // Creates a new instance object of this type and wraps it.
    Pngquant* obj = new Pngquant();

    obj->Wrap(args.This());

    NanReturnValue(args.This());
}


NAN_METHOD(Pngquant::Compress) {
    NanScope();

    struct rwpng_data * out_buffer;
    struct rwpng_data * in_buffer;

    if (args.Length() != 3) {
        return NanThrowTypeError(
            "Invalid argument, Need three arguments!"
        );
    }

    if (!Buffer::HasInstance(args[0])) {
        return NanThrowTypeError(
            "First argument must be a buffer."
        );
    }

    if (!args[2]->IsFunction()) {
        return NanThrowTypeError(
            "Third argument must be a callback function."
        );
    }

    png_bytep in_stream = (png_bytep) Buffer::Data(args[0]->ToObject());
    unsigned in_length = Buffer::Length(args[0]->ToObject());
    Local<String> opt = args[1]->ToString();
    Local<Function> callback = Local<Function>::Cast(args[2]);

    int i;
    char *argv[32];
    int argc;

    char str_buffer[BUFFER_SIZE];
    char buf[BUFFER_SIZE];
    memset(str_buffer, '\0', BUFFER_SIZE);
    memset(buf, '\0', BUFFER_SIZE);

    opt->WriteUtf8(str_buffer);
    strcpy(buf, "pngquant ");

    for (i = 0; i < 32; i++) {
        argv[i] = (char *)malloc(33);
    }

    strcat(buf, str_buffer);

    argc = parseArgv(buf, argv, 32);

    in_buffer = (struct rwpng_data *)malloc(sizeof(struct rwpng_data));
    if (in_buffer == NULL) {
        return NanThrowTypeError(
            "malloc fail!"
        );
    }
    out_buffer = (struct rwpng_data *)malloc(sizeof(struct rwpng_data));
    if (out_buffer == NULL) {
        return NanThrowTypeError(
            "malloc fail!"
        );
    }

    memset(out_buffer, '\0', sizeof(struct rwpng_data));

    in_buffer->png_data = in_stream;
    in_buffer->length = in_length;
    in_buffer->bytes_read = 0;

    int ret = pngquant(in_buffer, out_buffer, argc, argv);

    if (ret != 0) {
        out_buffer->png_data = in_buffer->png_data;
        out_buffer->length = in_buffer->length;
        fprintf(stderr, "File: %s\n", argv[argc-1]);
    }

    Local<Object> buffer = NanNewBufferHandle((char *)out_buffer->png_data, out_buffer->length);
    
    free(in_buffer);
    free(out_buffer);
    
    for (i = 0; i < 32; i++) {
        free(argv[i]);
    }

    NanReturnValue(buffer);
}

extern "C" {
    void RegisterModule(Handle<Object> exports) {
        Pngquant::Init(exports);
    }
}

NODE_MODULE(pngquant_native, RegisterModule);
