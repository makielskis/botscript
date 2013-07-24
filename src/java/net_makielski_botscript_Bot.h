/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class net_makielski_botscript_Bot */

#ifndef _Included_net_makielski_botscript_Bot
#define _Included_net_makielski_botscript_Bot
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     net_makielski_botscript_Bot
 * Method:    runService
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_net_makielski_botscript_Bot_runService
  (JNIEnv *, jclass);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    loadPackages
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_net_makielski_botscript_Bot_loadPackages
  (JNIEnv *, jclass, jstring);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    createIdentifier
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_createIdentifier
  (JNIEnv *, jclass, jstring, jstring, jstring);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    load
 * Signature: (Ljava/lang/String;Lnet/makielski/botscript/Bot/AsyncHandler;)V
 */
JNIEXPORT void JNICALL Java_net_makielski_botscript_Bot_load
  (JNIEnv *, jobject, jstring, jobject);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    identifier
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_identifier
  (JNIEnv *, jobject);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    u
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_u
  (JNIEnv *, jobject);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    p
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_p
  (JNIEnv *, jobject);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    s
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_s
  (JNIEnv *, jobject);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    configuration
 * Signature: (Z)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_net_makielski_botscript_Bot_configuration
  (JNIEnv *, jobject, jboolean);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    execute
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_net_makielski_botscript_Bot_execute
  (JNIEnv *, jobject, jstring, jstring);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    createBot
 * Signature: (Lnet/makielski/botscript/Bot/AsyncHandler;)J
 */
JNIEXPORT jlong JNICALL Java_net_makielski_botscript_Bot_createBot
  (JNIEnv *, jobject, jobject);

/*
 * Class:     net_makielski_botscript_Bot
 * Method:    shutdown
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_net_makielski_botscript_Bot_shutdown
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif