extern "C" {
  #include <png.h>
}
#include "pngquant/pngquant.h"
#include <node_buffer.h>
#include <stdlib.h>
#include <string.h>
#include "Compress.h"

namespace Compress {
  using v8::Local;
  using v8::FunctionCallbackInfo;
  using v8::Exception;
  using v8::Isolate;
  using v8::Value;
  using v8::String;
  using v8::Function;
  using v8::Object;
  using v8::Persistent;
  using v8::FunctionTemplate;
  using v8::MaybeLocal;

  #define ARGS_SIZE 1024
  typedef struct rwpng_data * rwpngTyp;

  Persistent<Function> Compress::constructor;

  Compress::Compress(char * buf, const int length, const char *args) {
    _buf = buf;
    _buf_length = length;
    _args = args;
  }

  Compress::~Compress() {
  }

  void Compress::Init(Isolate* isolate) {
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "Compress"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    // Prototype
    NODE_SET_PROTOTYPE_METHOD(tpl, "compress", Invoke);

    constructor.Reset(isolate, tpl->GetFunction());
  }

  void Compress::New(const FunctionCallbackInfo<Value> &args) {
    Isolate *isolate = args.GetIsolate();
    printf("%d", args.Length());
    if (args.IsConstructCall()) {
      char *buf = (char *) node::Buffer::Data(args[0]);
      unsigned buf_length = node::Buffer::Length(args[0]);
      Local<String> __args = args[1]->ToString();
      char __args_c[ARGS_SIZE];
      memset(__args_c, '\0', ARGS_SIZE);
      __args->WriteUtf8(__args_c);
      Compress *obj = new Compress(buf, buf_length, __args_c);
      obj->Wrap(args.This());
      args.GetReturnValue().Set(args.This());
    } else {
      const int argc = 2;
      Local<Value> argv[argc] = {args[0], args[1]};
      Local<Function> cons = Local<Function>::New(isolate, constructor);
      args.GetReturnValue().Set(cons->NewInstance(argc, argv));
    }
  }

  void Compress::NewInstance(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if (args.Length() < 2) {
      isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong number of arguments")));
      return;
    }

    if (!node::Buffer::HasInstance(args[0])) {
      isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "First argument must be a buffer.")));
      return;
    }

    const unsigned argc = 2;
    Local<Value> argv[argc] = { args[0], args[1]};
    Local<Function> cons = Local<Function>::New(isolate, constructor);
    Local<Object> instance = cons->NewInstance(argc, argv);

    args.GetReturnValue().Set(instance);
  }


  int Compress::ParseArgs(const char *args, int n, char *argv[]) {
    int i, len = (int)strlen(args), k = 0, argc = 0, is_new = 0;
    char *buf = (char *) malloc (n);
    memset(buf, '\0', n);
    for (i = 0; i < len + 1; i++) {
        if (args[i] != ' ' && i < len) {
            if (k < n) {
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

  void Compress::Invoke(const FunctionCallbackInfo<Value> &args) {
    rwpngTyp in = (rwpngTyp) malloc(sizeof(struct rwpng_data));
    rwpngTyp out = (rwpngTyp) malloc(sizeof(struct rwpng_data));
    int i;
    int argc;
    char *argv[32];
    Compress *obj = ObjectWrap::Unwrap<Compress>(args.Holder());

    // invoke pngquant compress buffer
    char c_args[ARGS_SIZE];
    memset(c_args, '\0', ARGS_SIZE);
    strcpy(c_args, "pngqaunt ");
    strcat(c_args, obj->_args);


    for (i = 0; i < 32; i++) { argv[i] = (char *)malloc(33); } //new argv

    argc = ParseArgs(c_args, 32, argv);

    //init
    in->png_data = (png_bytep) obj->_buf;
    in->length = obj->_buf_length;
    in->bytes_read = 0;

    //reset
    memset(out, '\0', sizeof(struct rwpng_data));

    int ret = pngquant(in, out, argc, argv);

    if (ret != 0) {
      out->png_data = in->png_data;
      out->length = in->length;
      fprintf(stderr, "\tFile: %s\n", argv[argc-1]);
    }

    MaybeLocal<Object> maybeBuffer = node::Buffer::New(args.GetIsolate(), (char *)out->png_data, (size_t)out->length);
    Local<Object> buffer;
    maybeBuffer.ToLocal(&buffer);

    for (i = 0; i < 32; i++) { free(argv[i]); } //free argv
    free(in);
    free(out);

    args.GetReturnValue().Set(buffer);
  }
}