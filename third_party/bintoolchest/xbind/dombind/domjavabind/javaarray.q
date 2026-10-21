# javaarray.q — java C API primitives.
@module javaarray
@version 1.0
@param PREFIX javaarray
@include <jni.h>
@guard

@fn size {
static inline jsize ${PREFIX}_size(JNIEnv *env, jarray a) {
    
#ifdef __cplusplus
    return env->GetArrayLength(a);
#else
    return (*env)->GetArrayLength(env, a);
#endif
}
}

@fn bytes {
static inline jbyteArray ${PREFIX}_bytes(JNIEnv *env, jsize n) {
    
#ifdef __cplusplus
    return env->NewByteArray(n);
#else
    return (*env)->NewByteArray(env, n);
#endif
}
}

@fn bytes_get {
static inline void ${PREFIX}_bytes_get(JNIEnv *env, jbyteArray a, jsize start, jsize n, jbyte *out) {
    
#ifdef __cplusplus
    env->GetByteArrayRegion(a, start, n, out);
#else
    (*env)->GetByteArrayRegion(env, a, start, n, out);
#endif
}
}

@fn bytes_set {
static inline void ${PREFIX}_bytes_set(JNIEnv *env, jbyteArray a, jsize start, jsize n, const jbyte *data) {
    
#ifdef __cplusplus
    env->SetByteArrayRegion(a, start, n, data);
#else
    (*env)->SetByteArrayRegion(env, a, start, n, data);
#endif
}
}

@fn objects {
static inline jobjectArray ${PREFIX}_objects(JNIEnv *env, jsize n, jclass type, jobject initial) {
    
#ifdef __cplusplus
    return env->NewObjectArray(n, type, initial);
#else
    return (*env)->NewObjectArray(env, n, type, initial);
#endif
}
}

@fn get {
static inline jobject ${PREFIX}_get(JNIEnv *env, jobjectArray a, jsize i) {
    
#ifdef __cplusplus
    return env->GetObjectArrayElement(a, i);
#else
    return (*env)->GetObjectArrayElement(env, a, i);
#endif
}
}

@fn set {
static inline void ${PREFIX}_set(JNIEnv *env, jobjectArray a, jsize i, jobject v) {
    
#ifdef __cplusplus
    env->SetObjectArrayElement(a, i, v);
#else
    (*env)->SetObjectArrayElement(env, a, i, v);
#endif
}
}

