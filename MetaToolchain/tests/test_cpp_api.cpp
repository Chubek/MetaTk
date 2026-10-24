#include "MetaArchAPI.hpp"
#include <iostream>
#include <type_traits>

using metaarch::api::Spec;
static_assert(!std::is_copy_constructible<Spec>::value, "spec must not double-free");
static_assert(std::is_nothrow_move_constructible<Spec>::value, "movable RAII handle");

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    auto check = [](bool ok) { if (!ok) throw std::runtime_error("C++ API assertion failed"); };
    try {
        auto s = Spec::load(argv[1], MA_AUTO, MA_LOAD_LINT | MA_LOAD_STRICT);
        check(s.diagnostics().empty() && s.target_count() == 2 && s.register_count() == 2);
        check(s.encoder_count() == 3 && s.form_count() == 3);
        check(s.target(0).arch == "test" && s.target(1).byte_order == MA_BIG_ENDIAN);
        check(s.reg("r1").encoding == 1 && s.reg(size_t(0)).reg_class == "gpr");
        check(s.encoder_name(0) == "pair" && s.find_encoder("branch") == 1);
        check(s.form(0).visible_operand_count == 2);
        auto out = s.encode("pair", {{"dst", 1}, {"imm", -1}});
        check(out.bytes == std::vector<uint8_t>({1, 255, 0, 0}));
        check(s.encode_form(0, {1, -1}, 1).bytes == std::vector<uint8_t>({0, 0, 255, 1}));
        bool failed = false;
        try { s.encode_form(0, {1, 11}); } catch (const metaarch::api::Error &e) {
            failed = !e.diagnostics().empty() && std::string(e.what()).find("operand 3 (imm)") != std::string::npos;
        }
        check(failed);
        for (MaFormat f = MA_YAML; f <= MA_SEXPR; ++f) {
            auto text = s.write(f, MA_WRITE_REF_BACKEND);
            auto copy = Spec::parse(text, f);
            check(copy.encoder_count() == 3);
        }
        s.load_backend();
        auto relocation = s.encode("branch", {{"symbol", 42}});
        auto moved = std::move(s);
        failed = false;
        try { s.form_count(); } catch (const std::logic_error &) { failed = true; }
        check(failed);
        moved.close(); moved.close();
        check(relocation.relocations[0].name == "R_FIX" && relocation.relocations[0].bit_offset == 5 &&
              relocation.relocations[0].bit_width == 20 && relocation.relocations[0].byte_order == MA_BIG_ENDIAN);
        failed = false;
        try { Spec::parse("bad: [", MA_YAML); } catch (const metaarch::api::Error &e) { failed = !e.diagnostics().empty(); }
        check(failed);
        failed = false;
        try { Spec::load(std::string("bad\0path", 8)); } catch (const std::invalid_argument &) { failed = true; }
        check(failed);
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "C++17 ABI wrapper tests passed\n";
}
