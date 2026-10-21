# javastr.q — java C API primitives.
@module javastr
@version 1.0
@param PREFIX javastr
@include <jni.h>
@guard

@fn new {
static inline jstring ${PREFIX}_new(JNIEnv *env, const jchar *s, jsize n) {
    
#ifdef __cplusplus
    return env->NewString(s, n);
#else
    return (*env)->NewString(env, s, n);
#endif
}
}

@fn size {
static inline jsize ${PREFIX}_size(JNIEnv *env, jstring s) {
    
#ifdef __cplusplus
    return env->GetStringLength(s);
#else
    return (*env)->GetStringLength(env, s);
#endif
}
}

@fn acquire {
static inline const jchar * ${PREFIX}_acquire(JNIEnv *env, jstring s, jboolean *copy) {
    
#ifdef __cplusplus
    return env->GetStringChars(s, copy);
#else
    return (*env)->GetStringChars(env, s, copy);
#endif
}
}

@fn release {
static inline void ${PREFIX}_release(JNIEnv *env, jstring s, const jchar *data) {
    
#ifdef __cplusplus
    env->ReleaseStringChars(s, data);
#else
    (*env)->ReleaseStringChars(env, s, data);
#endif
}
}

@fn modified_utf8 {
static inline jstring ${PREFIX}_modified_utf8(JNIEnv *env, const char *s) {
    
#ifdef __cplusplus
    return env->NewStringUTF(s);
#else
    return (*env)->NewStringUTF(env, s);
#endif
}
}

