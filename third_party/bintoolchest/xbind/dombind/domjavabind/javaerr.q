# javaerr.q — java C API primitives.
@module javaerr
@version 1.0
@param PREFIX javaerr
@include <jni.h>
@guard

@fn pending {
static inline jboolean ${PREFIX}_pending(JNIEnv *env) {
    
#ifdef __cplusplus
    return env->ExceptionCheck();
#else
    return (*env)->ExceptionCheck(env);
#endif
}
}

@fn get {
static inline jthrowable ${PREFIX}_get(JNIEnv *env) {
    
#ifdef __cplusplus
    return env->ExceptionOccurred();
#else
    return (*env)->ExceptionOccurred(env);
#endif
}
}

@fn clear {
static inline void ${PREFIX}_clear(JNIEnv *env) {
    
#ifdef __cplusplus
    env->ExceptionClear();
#else
    (*env)->ExceptionClear(env);
#endif
}
}

@fn throw {
static inline jint ${PREFIX}_throw(JNIEnv *env, jclass cls, const char *message) {
    
#ifdef __cplusplus
    return env->ThrowNew(cls, message);
#else
    return (*env)->ThrowNew(env, cls, message);
#endif
}
}

