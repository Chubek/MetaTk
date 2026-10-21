#include <domrubybind/domrubybind.hpp>
int main(int argc, char **argv) {
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (rubyinit_setup() != 0) return 1;
    int status = 0;
    {
        try {
            auto result = domrubybind::eval("6 * 7");
            status = rubyconv_to_int64(result.get()) == 42 ? 0 : 1;
        } catch (const domrubybind::error &) { status = 1; }
    }
    return rubyinit_cleanup(status);
}
