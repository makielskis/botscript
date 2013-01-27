#include "./net_makielski_botscript_Bot.h"

#include <iostream>
#include <string>

#include "boost/asio/io_service.hpp"

#include "./util.h"

using namespace botscript;

boost::asio::io_service io_service;

JNIEXPORT void JNICALL Java_net_makielski_botscript_Bot_runService
    (JNIEnv* env, jclass) {
  boost::asio::io_service::work work(io_service);
  io_service.run();
}

JNIEXPORT jobjectArray JNICALL Java_net_makielski_botscript_Bot_loadPackages
  (JNIEnv* env, jclass, jstring path) {
  // Get path to load the packages from.
  const char* cpath = env->GetStringUTFChars(path, 0);
  std::string p = cpath;
  env->ReleaseStringUTFChars(path, cpath);

  // Get packages.
  std::vector<std::string> packages = bot::load_packages(p);

  // Package packages vector to jobjectArray.
  jclass str_cls = env->FindClass("java/lang/String");
  jobjectArray result = env->NewObjectArray(packages.size(), str_cls, 0);
  jsize i = 0;
  for (const std::string& package : packages) {
    env->SetObjectArrayElement(result, i,
                               env->NewStringUTF(package.c_str()));
    ++i;
  }

  return result;
}

JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_createIdentifier
    (JNIEnv* env, jclass, jstring u, jstring p, jstring s) {
  // Read parameters.
  const char* cu = env->GetStringUTFChars(u, 0);
  const char* cp = env->GetStringUTFChars(p, 0);
  const char* cs = env->GetStringUTFChars(s, 0);
  std::string username = cu;
  std::string package = cp;
  std::string server = cs;
  env->ReleaseStringUTFChars(u, cu);
  env->ReleaseStringUTFChars(p, cp);
  env->ReleaseStringUTFChars(s, cs);

  // Get identifier.
  std::string id = botscript::bot::identifier(username, package, server);
  return env->NewStringUTF(id.c_str());
}

JNIEXPORT void JNICALL Java_net_makielski_botscript_Bot_load
    (JNIEnv* env, jobject obj, jstring configuration, jobject handler) {
  // Read configuration C string
  const char* cconfiguration = env->GetStringUTFChars(configuration, 0);
  std::string config = cconfiguration;
  env->ReleaseStringUTFChars(configuration, cconfiguration);

  // Get bot address from field and call execute.
  jni_bot* jni_b = get_handle(env, obj);
  if (jni_b == nullptr) {
    return;
  }
  botscript::bot* b = jni_b->get();

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

  b->init(config, [jvm, g_handler](std::shared_ptr<bot> bot,
                                   std::string err) {
    // Attach thread.
    JNIEnv* env = nullptr;
    jint res;
    res = jvm->AttachCurrentThread((void**) &env, nullptr);
    if(res < 0) {
      return;
    }

    // Prepare function argument.
    jstring j_err = env->NewStringUTF(err.c_str());

    // Call function.
    jclass cls = env->GetObjectClass(g_handler);
    jmethodID mid = env->GetMethodID(cls, "call", "(Ljava/lang/String;)V");
    if (mid == nullptr) {
      return;
    }
    env->CallVoidMethod(g_handler, mid, j_err);

    // Detach thread.
    jvm->DetachCurrentThread();
  });
}

JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_identifier
    (JNIEnv* env, jobject obj) {
  jni_bot* b = get_handle(env, obj);
  if (nullptr == b) {
    return env->NewStringUTF("");
  }
  return env->NewStringUTF(b->get()->identifier().c_str());
}

JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_u
    (JNIEnv* env, jobject obj) {
  jni_bot* b = get_handle(env, obj);
  if (nullptr == b) {
    return env->NewStringUTF("");
  }
  return env->NewStringUTF(b->get()->username().c_str());
}

JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_p
    (JNIEnv* env, jobject obj) {
  jni_bot* b = get_handle(env, obj);
  if (nullptr == b) {
    return env->NewStringUTF("");
  }
  return env->NewStringUTF(b->get()->package().c_str());
}

JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_s
    (JNIEnv* env, jobject obj) {
  jni_bot* b = get_handle(env, obj);
  if (nullptr == b) {
    return env->NewStringUTF("");
  }
  return env->NewStringUTF(b->get()->server().c_str());
}

JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_configuration
    (JNIEnv* env, jobject obj, jboolean with_pw) {
  jni_bot* b = get_handle(env, obj);
  if (nullptr == b) {
    return env->NewStringUTF("");
  }
  return env->NewStringUTF(b->get()->configuration(with_pw).c_str());
}

JNIEXPORT void JNICALL Java_net_makielski_botscript_Bot_execute
    (JNIEnv* env, jobject obj, jstring command, jstring argument) {
  // Get comand and argument as C string.
  const char* ccommand = env->GetStringUTFChars(command, 0);
  const char* cargument = env->GetStringUTFChars(argument, 0);

  // Wrap command and argument in C++ string.
  std::string c = ccommand, a = cargument;

  // Release command and argument C string.
  env->ReleaseStringUTFChars(command, ccommand);
  env->ReleaseStringUTFChars(argument, cargument);

  // Get bot address from field and call execute.
  jni_bot* b = get_handle(env, obj);
  if (nullptr == b) {
    return;
  }
  b->get()->execute(c, a);
}

JNIEXPORT void JNICALL Java_net_makielski_botscript_Bot_shutdown
    (JNIEnv* env, jobject obj) {
  jni_bot* b = get_handle(env, obj);
  if (nullptr == b) {
    return;
  }
  b->get()->shutdown();
  delete b;
}

JNIEXPORT jlong JNICALL Java_net_makielski_botscript_Bot_createBot
    (JNIEnv* env, jobject obj, jobject handler) {
  jni_bot* b = new jni_bot(&io_service);
  set_update_handler(b, env, obj, handler);
  return reinterpret_cast<long>(b);
}
