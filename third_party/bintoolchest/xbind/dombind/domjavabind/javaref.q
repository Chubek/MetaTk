# javaref.q — java C API primitives.
@module javaref
@version 1.0
@param PREFIX javaref
@include <jni.h>
@guard

@fn retain {
static inline jobject ${PREFIX}_retain(JNIEnv *env, jobject obj) {
    
#ifdef __cplusplus
    return env->NewGlobalRef(obj);
#else
    return (*env)->NewGlobalRef(env, obj);
#endif
}
}

@fn release {
static inline void ${PREFIX}_release(JNIEnv *env, jobject obj) {
    
#ifdef __cplusplus
    env->DeleteGlobalRef(obj);
#else
    (*env)->DeleteGlobalRef(env, obj);
#endif
}
}

@fn local_release {
static inline void ${PREFIX}_local_release(JNIEnv *env, jobject obj) {
    
#ifdef __cplusplus
    env->DeleteLocalRef(obj);
#else
    (*env)->DeleteLocalRef(env, obj);
#endif
}
}

@fn frame_push {
static inline jint ${PREFIX}_frame_push(JNIEnv *env, jint capacity) {
    
#ifdef __cplusplus
    return env->PushLocalFrame(capacity);
#else
    return (*env)->PushLocalFrame(env, capacity);
#endif
}
}

@fn frame_pop {
static inline jobject ${PREFIX}_frame_pop(JNIEnv *env, jobject result) {
    
#ifdef __cplusplus
    return env->PopLocalFrame(result);
#else
    return (*env)->PopLocalFrame(env, result);
#endif
}
}

@fn same {
static inline jboolean ${PREFIX}_same(JNIEnv *env, jobject a, jobject b) {
    
#ifdef __cplusplus
    return env->IsSameObject(a, b);
#else
    return (*env)->IsSameObject(env, a, b);
#endif
}
}

