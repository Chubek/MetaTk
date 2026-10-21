# javacall.q — java C API primitives.
@module javacall
@version 1.0
@param PREFIX javacall
@include <jni.h>
@guard

@fn find_class {
static inline jclass ${PREFIX}_find_class(JNIEnv *env, const char *name) {
    
#ifdef __cplusplus
    return env->FindClass(name);
#else
    return (*env)->FindClass(env, name);
#endif
}
}

@fn method {
static inline jmethodID ${PREFIX}_method(JNIEnv *env, jclass cls, const char *name, const char *sig) {
    
#ifdef __cplusplus
    return env->GetMethodID(cls, name, sig);
#else
    return (*env)->GetMethodID(env, cls, name, sig);
#endif
}
}

@fn static_method {
static inline jmethodID ${PREFIX}_static_method(JNIEnv *env, jclass cls, const char *name, const char *sig) {
    
#ifdef __cplusplus
    return env->GetStaticMethodID(cls, name, sig);
#else
    return (*env)->GetStaticMethodID(env, cls, name, sig);
#endif
}
}

@fn construct {
static inline jobject ${PREFIX}_construct(JNIEnv *env, jclass cls, jmethodID method, const jvalue *args) {
    
#ifdef __cplusplus
    return env->NewObjectA(cls, method, args);
#else
    return (*env)->NewObjectA(env, cls, method, args);
#endif
}
}

@fn register {
static inline jint ${PREFIX}_register(JNIEnv *env, jclass cls, const JNINativeMethod *methods, jint n) {
    
#ifdef __cplusplus
    return env->RegisterNatives(cls, methods, n);
#else
    return (*env)->RegisterNatives(env, cls, methods, n);
#endif
}
}

@fn object {
static inline jobject ${PREFIX}_object(JNIEnv *env, jobject obj, jmethodID method, const jvalue *args) {
    
#ifdef __cplusplus
    return env->CallObjectMethodA(obj, method, args);
#else
    return (*env)->CallObjectMethodA(env, obj, method, args);
#endif
}
}

@fn integer {
static inline jint ${PREFIX}_integer(JNIEnv *env, jobject obj, jmethodID method, const jvalue *args) {
    
#ifdef __cplusplus
    return env->CallIntMethodA(obj, method, args);
#else
    return (*env)->CallIntMethodA(env, obj, method, args);
#endif
}
}

@fn long {
static inline jlong ${PREFIX}_long(JNIEnv *env, jobject obj, jmethodID method, const jvalue *args) {
    
#ifdef __cplusplus
    return env->CallLongMethodA(obj, method, args);
#else
    return (*env)->CallLongMethodA(env, obj, method, args);
#endif
}
}

@fn double {
static inline jdouble ${PREFIX}_double(JNIEnv *env, jobject obj, jmethodID method, const jvalue *args) {
    
#ifdef __cplusplus
    return env->CallDoubleMethodA(obj, method, args);
#else
    return (*env)->CallDoubleMethodA(env, obj, method, args);
#endif
}
}

@fn void {
static inline void ${PREFIX}_void(JNIEnv *env, jobject obj, jmethodID method, const jvalue *args) {
    
#ifdef __cplusplus
    env->CallVoidMethodA(obj, method, args);
#else
    (*env)->CallVoidMethodA(env, obj, method, args);
#endif
}
}

