#include <png.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pngquant/pngquant.h"
#include "PQCompress.h"

#define ARGS_SIZE 1024
typedef struct rwpng_data * rwpngTyp;

napi_ref PQCompress::constructor;

PQCompress::PQCompress(char * buf, const unsigned length, const char *args) {
  _buf = buf;
  _buf_length = length;
  _args = args;
  env_ = nullptr;
  wrapper_ = nullptr;
}

PQCompress::~PQCompress() {
  napi_delete_reference(env_, wrapper_);
}

void PQCompress::Destructor(napi_env env, void* nativeObject, void* /*finalize_hint*/) {
  reinterpret_cast<PQCompress*>(nativeObject)->~PQCompress();
}

napi_value PQCompress::Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_property_descriptor properties[] = {
      { "compress", 0, Invoke, 0, 0, 0, napi_default, 0 }
  };

  napi_value cons;

  status = napi_define_class(env, "PQCompress", NAPI_AUTO_LENGTH, New, nullptr, 1, properties, &cons);
  assert(status == napi_ok);

  status = napi_create_reference(env, cons, 1, &constructor);
  assert(status == napi_ok);

  status = napi_set_named_property(env, exports, "Pngquant", cons);
  assert(status == napi_ok);
  return exports;
}

napi_value PQCompress::New(napi_env env, napi_callback_info info) {
  napi_status status;
  napi_value target = nullptr;

  status = napi_get_new_target(env, info, &target);
  assert(status == napi_ok);
  bool is_constructor = target != nullptr;

  if (is_constructor) {
    size_t argc = 2;
    napi_value args[argc], jsthis;
    char *data;
    size_t length;


    char line_args[ARGS_SIZE];
    PQCompress* obj;

    status = napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
    assert(status == napi_ok);

    if (argc >= 2) {
      status = napi_get_buffer_info(env, args[0], (void **)&data, &length);
      assert(status == napi_ok);

      status = napi_get_value_string_utf8(env, args[1], line_args, ARGS_SIZE, nullptr);
      //printf("c_args = %s", line_args);

      obj = new PQCompress(data, (const unsigned)length, line_args);
    } else {
      napi_throw_error(env, "50000", "ERROR: invalid arguments, `new _handle.Pngquant(<Buffer>, <char *>)");
      return nullptr;
    }

    obj->env_ = env;

    status = napi_wrap(env,
                       jsthis,
                       reinterpret_cast<void*>(obj),
                       PQCompress::Destructor,
                       nullptr,  // finalize_hint
                       &obj->wrapper_);
    assert(status == napi_ok);
  } else {
    napi_throw_error(env, "50000", "ERROR: invalid call, Please use new PQCompress(...).");
  }
}

int PQCompress::ParseArgs(const char *args, int n, char *argv[]) {
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

napi_value PQCompress::Invoke(napi_env env, napi_callback_info info) {
  rwpngTyp in = (rwpngTyp) malloc(sizeof(struct rwpng_data));
  rwpngTyp out = (rwpngTyp) malloc(sizeof(struct rwpng_data));
  int i;
  int cmd_argc;
  char *cmd_argv[32];

  napi_status status;
  napi_value jsthis;
  PQCompress *obj;

  status = napi_get_cb_info(env, info, nullptr, nullptr, &jsthis, nullptr);
  assert(status == napi_ok);

  status = napi_unwrap(env, jsthis, reinterpret_cast<void**>(&obj));
  assert(status == napi_ok);

  // invoke pngquant compress buffer
  char c_args[ARGS_SIZE];
  memset(c_args, '\0', ARGS_SIZE);
  strcpy(c_args, "-f pngqaunt ");
  strcat(c_args, obj->_args);


  for (i = 0; i < 32; i++) { cmd_argv[i] = (char *)malloc(33); } //new argv

  cmd_argc = ParseArgs(c_args, 32, cmd_argv);

  //init
  in->png_data = (png_bytep) obj->_buf;
  in->length = obj->_buf_length;
  in->bytes_read = 0;

  //reset
  memset(out, '\0', sizeof(struct rwpng_data));

  int ret = pngquant(in, out, cmd_argc, cmd_argv);

  if (ret != 0) {
    out->png_data = in->png_data;
    out->length = in->length;

    napi_throw_error(env, "50000", cmd_argv[cmd_argc - 1]);
  }

  napi_value result;

  status = napi_create_buffer_copy(env, (size_t) out->length, (char *) out->png_data, nullptr, &result);
  assert(status == napi_ok);

  for (i = 0; i < 32; i++) { free(cmd_argv[i]); }
  free(in);
  free(out);

  return result;
}