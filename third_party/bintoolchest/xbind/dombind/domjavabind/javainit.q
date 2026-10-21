# javainit.q — java C API primitives.
@module javainit
@version 1.0
@param PREFIX javainit
@include <jni.h>
@guard

@fn create {
static inline jint ${PREFIX}_create(JavaVM **vm, JNIEnv **env, JavaVMInitArgs *args) {
    return JNI_CreateJavaVM(vm, (void **)env, args);
}
}

@fn attach {
static inline jint ${PREFIX}_attach(JavaVM *vm, JNIEnv **env) {
    
#ifdef __cplusplus
    return vm->AttachCurrentThread((void **)env, NULL);
#else
    return (*vm)->AttachCurrentThread(vm, (void **)env, NULL);
#endif
}
}

@fn detach {
static inline jint ${PREFIX}_detach(JavaVM *vm) {
    
#ifdef __cplusplus
    return vm->DetachCurrentThread();
#else
    return (*vm)->DetachCurrentThread(vm);
#endif
}
}

@fn destroy {
static inline jint ${PREFIX}_destroy(JavaVM *vm) {
    
#ifdef __cplusplus
    return vm->DestroyJavaVM();
#else
    return (*vm)->DestroyJavaVM(vm);
#endif
}
}

