extern "C" {
#include "pms7003.h"
}

#include "binding_utils.h"

#include <napi.h>

Napi::Object init(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  int err = PMS7003_init();
  if (err) {
    return BindingUtils::errFactory(env, err,
      "Could not initialize PMS7003 module :(");
  }

  Napi::Object returnObject = Napi::Object::New(env);
  returnObject.Set(Napi::String::New(env, "returnCode"), Napi::Number::New(env, err));
  return returnObject;
}

Napi::Object deinit(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  int err = PMS7003_deinit();
  if (err) {
    return BindingUtils::errFactory(env, err,
      "Could not de-initialize PMS7003 module :(");
  }

  Napi::Object returnObject = Napi::Object::New(env);
  returnObject.Set(Napi::String::New(env, "returnCode"), Napi::Number::New(env, err));
  return returnObject;
}

Napi::Object read(const Napi::CallbackInfo &info) {
  // Get arguments
  int timeout = info[0].As<Napi::Number>();
  Napi::Env env = info.Env();

  // Read data
  pms7003_data data;
  int err = PMS7003_read(timeout, &data);
  if (err) {
    PMS7003_deinit();
    return BindingUtils::errFactory(env, err, "Failed to read data");
  }

  // Put return values into an object
  Napi::Object returnObject = Napi::Object::New(env);
  returnObject.Set(Napi::String::New(env, "pm1_s"), Napi::Number::New(env, data.pm1_0_s));
  returnObject.Set(Napi::String::New(env, "pm2_5_s"), Napi::Number::New(env, data.pm2_5_s));
  returnObject.Set(Napi::String::New(env, "pm10_s"), Napi::Number::New(env, data.pm10_s));
  returnObject.Set(Napi::String::New(env, "pm1_0"), Napi::Number::New(env, data.pm1_0));
  returnObject.Set(Napi::String::New(env, "pm2_5"), Napi::Number::New(env, data.pm2_5));
  returnObject.Set(Napi::String::New(env, "pm10"), Napi::Number::New(env, data.pm10));
  returnObject.Set(Napi::String::New(env, "bucket0_3"), Napi::Number::New(env, data.bucket0_3));
  returnObject.Set(Napi::String::New(env, "bucket0_5"), Napi::Number::New(env, data.bucket0_5));
  returnObject.Set(Napi::String::New(env, "bucket1_0"), Napi::Number::New(env, data.bucket1_0));
  returnObject.Set(Napi::String::New(env, "bucket2_5"), Napi::Number::New(env, data.bucket2_5));
  returnObject.Set(Napi::String::New(env, "bucket_5_0"), Napi::Number::New(env, data.bucket5_0));
  returnObject.Set(Napi::String::New(env, "bucket_10"), Napi::Number::New(env, data.bucket10));
  return returnObject;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "init"),
              Napi::Function::New(env, init));
  exports.Set(Napi::String::New(env, "deinit"),
              Napi::Function::New(env, deinit));
  exports.Set(Napi::String::New(env, "read"),
              Napi::Function::New(env, read));
  return exports;
}

NODE_API_MODULE(homebridgepms7003, Init)
