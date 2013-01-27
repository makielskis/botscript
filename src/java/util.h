#ifndef BS_JAVA_UTIL_H_
#define BS_JAVA_UTIL_H_

#include <jni.h>

#include <memory>

#include "boost/asio/io_service.hpp"

#include "../bot.h"

/// Wrapper class to be used in the Java Native Interface.
/// Contains a std::shared_ptr pointing to the wrapped bot.
/// This is required for the bot to call shared_from_this().
class jni_bot {
 public:
  /// Constructor.
  ///
  /// \param io_service the Asio io_service
  jni_bot(boost::asio::io_service* io_service)
      : bot_(std::make_shared<botscript::bot>(io_service)) {
  }

  /// \return the wrapped bot.
  botscript::bot* get() {
    return bot_.get();
  }

 private:
  /// The wrapped bot.
  std::shared_ptr<botscript::bot> bot_;
};

/// Sets the bot update callback to the updateHandler variable.
///
/// \param env the JNI environment pointer
/// \param obj the JNI Bot object
void set_update_handler(jni_bot* b, JNIEnv* env, jobject obj, jobject handler) {
  // Prepare global reference to handler and pointer to the JVM.
  JavaVM* jvm = nullptr;
  jobject g_handler = nullptr;

  // Get JVM.
  env->GetJavaVM(&jvm);
  if (nullptr == jvm) {
    return;
  }

  // Get global reference to handler object.
  g_handler = env->NewGlobalRef(handler);
  if (nullptr == g_handler) {
    return;
  }

  // Set callback to call the updateHandler.call method.
  b->get()->callback_ = [jvm, g_handler](std::string id,
                                         std::string k,
                                         std::string v) {
    // Build update string.
    std::string update = id + "|" + k + "|" + v;

    // Attach thread.
    JNIEnv* env = nullptr;
    jint res;
    res = jvm->AttachCurrentThread((void**) &env, nullptr);
    if(res < 0) {
      return;
    }

    // Prepare function argument.
    jstring j_err = env->NewStringUTF(update.c_str());

    // Call function.
    jclass cls = env->GetObjectClass(g_handler);
    jmethodID mid = env->GetMethodID(cls, "call", "(Ljava/lang/String;)V");
    if (mid == nullptr) {
      return;
    }
    env->CallVoidMethod(g_handler, mid, j_err);

    // Detach thread.
    jvm->DetachCurrentThread();
  };
}

/// Extracts a handle to a bot wrapped in the Bot JNI Java class.
///
/// \param env the JNI environment pointer
/// \param obj the JNI Bot object
jni_bot* get_handle(JNIEnv* env, jobject obj) {
  // Get class reference.
  jclass cls = env->GetObjectClass(obj);

  // Get bot field in the Bot object.
  jfieldID fid = env->GetFieldID(cls, "bot", "J");
  if (fid == nullptr) {
    return nullptr;
  }

  // Extract, cast and return the bot stored bot handle.
  return reinterpret_cast<jni_bot*>(env->GetLongField(obj, fid));
}

#endif
