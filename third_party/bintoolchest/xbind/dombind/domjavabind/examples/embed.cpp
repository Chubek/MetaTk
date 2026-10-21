#include <domjavabind/domjavabind.hpp>
int main() {
    JavaVM *vm = nullptr;
    JNIEnv *env = nullptr;
    JavaVMInitArgs args = {};
    args.version = JNI_VERSION_1_8;
    if (javainit_create(&vm, &env, &args) != JNI_OK) return 1;
    int status = 0;
    {
        domjavabind::local_frame frame(env);
        jchar text[] = {'o', 'k'};
        jstring str = javastr_new(env, text, 2);
        if (javaerr_pending(env)) { javaerr_clear(env); status = 1; }
        else status = javastr_size(env, str) == 2 ? 0 : 1;
    }
    return javainit_destroy(vm) == JNI_OK ? status : 1;
}
