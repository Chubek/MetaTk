#include "MetaAS/MetaAS.hpp"
#include "MetaLD/MetaLD.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

namespace {
unsigned checks;
void check(bool value) { ++checks; if (!value) throw std::runtime_error("embedding assertion " + std::to_string(checks)); }
template<class F> void rejects(F f, std::string_view message) {
    try { f(); } catch (const std::exception &e) { check(std::string_view(e.what()).find(message) != std::string_view::npos); return; }
    throw std::runtime_error("expected error: " + std::string(message));
}
}
int main(int argc, char **argv) {
    try {
        check(argc == 3);
        std::filesystem::path root = argv[1], work = argv[2];
        std::string database = (root / "MetaArchDB").string(), catalogue = (root / "ArchBFDCatalogue.yaml").string();
        auto a = metaas::options(); a.target = "amd64"; a.database = database.c_str(); a.backend = METAAS_GAS;
        metaas::Assembler assembler(a);
        int invoked = 0;
        assembler.directive("exit", [&](std::string_view, const ArchASInvocation &inv) {
            check(inv.line == 4); ++invoked;
            return "mov $exit_status,%edi\nmov $60,%eax\nsyscall\n";
        });
        assembler.directive("bad", [](auto, auto &) -> std::string { throw std::runtime_error("directive boom"); });
        auto moved = std::move(assembler);
        rejects([&] { moved.compile(".bad"); }, "directive boom");
        auto object = moved.compile(".text\n.globl _start\n_start:\n.exit\n");
        check(invoked == 1 && !object.bytes().empty());
        auto l = metald::options(); l.catalogue = catalogue.c_str();
        metald::Linker linker(l);
        linker.add("in-memory.o", object.bytes()).resolver([](std::string_view name) -> std::optional<uint64_t> {
            if (name == "exit_status") return 37;
            return {};
        });
        auto linked = std::move(linker);
        linked.link();
        check(linked.symbol("exit_status") == 37 && linked.symbol("_start").has_value());
        check(!linked.symbol("absent") && linked.image().size() > 10);
        check(linked.symbols().size() == 2);
        std::atomic<bool> good{true};
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) threads.emplace_back([&] {
            for (int n = 0; n < 1000; ++n) if (linked.symbol("exit_status") != 37 || linked.image().empty()) good = false;
        });
        for (auto &t : threads) t.join();
        check(good);
        linked.write_executable((work / "cpp-linked").string());
        linked.write_map((work / "cpp-linked.map").string());
        rejects([&] { linked.define("late", 1); }, "state");
        metald::Linker unknown(l);
        unknown.add("unknown.o", object.bytes());
        rejects([&] { unknown.link(); }, "undefined reference to exit_status");
        metald::Linker failure(l);
        failure.add("exception.o", object.bytes()).resolver([](auto) -> std::optional<uint64_t> { throw std::runtime_error("resolver boom"); });
        rejects([&] { failure.link(); }, "resolver boom");
        check(failure.state() == METALD_FAILED);
        metald::Linker duplicate(l);
        duplicate.define("_start", 1).add("duplicate.o", object.bytes());
        rejects([&] { duplicate.link(); }, "duplicate strong definition");
        metald::Linker twice(l);
        twice.define("host", 1);
        rejects([&] { twice.define("host", 2); }, "duplicate host symbol");
        metald::Linker reserved(l);
        rejects([&] { reserved.define("_GLOBAL_OFFSET_TABLE_", 1); }, "reserved linker symbol");

        // Archive definitions are preferred over the lazy resolver. Explicit
        // host definitions suppress extraction of a member defining that name.
        auto entry = moved.compile(".text\n.globl _start\n_start: call helper\nmov $60,%eax\nmov $37,%edi\nsyscall\n");
        // The Python driver supplies a real GNU ar archive defining helper.
        metald::Linker archived(l);
        int resolved = 0;
        archived.add("entry.o", entry.bytes()).add((work / "helper.a").string()).resolver([&](auto) -> std::optional<uint64_t> { ++resolved; return {}; }).link();
        check(resolved == 0 && archived.symbol("helper").has_value());
        archived.write_executable((work / "archive-linked").string());
        metald::Linker host(l);
        host.define("helper", 0x401000).add("entry.o", entry.bytes()).add((work / "helper.a").string()).link();
        check(host.symbol("helper") == 0x401000);
        std::ifstream archive(work / "helper.a", std::ios::binary);
        check(bool(archive));
        std::vector<uint8_t> archive_bytes{std::istreambuf_iterator<char>(archive), std::istreambuf_iterator<char>()};
        metald::Linker memory_archive(l);
        memory_archive.add("entry.o", entry.bytes()).add("copied.a", archive_bytes);
        archive_bytes.clear(); archive_bytes.shrink_to_fit();
        memory_archive.link();
        check(memory_archive.symbol("helper") == archived.symbol("helper"));
        metald::Linker overflow(l);
        overflow.define("helper", UINT64_C(1) << 40).add("range.o", entry.bytes());
        rejects([&] { overflow.link(); }, "relocation overflow: 32-bit field");

        auto weak = moved.compile(".weak absent_weak\n.text\n.globl _start\n_start: ret\n.data\n.globl slot\nslot: .quad absent_weak\n");
        metald::Linker weak_link(l);
        resolved = 0;
        weak_link.add("weak.o", weak.bytes()).resolver([&](auto) -> std::optional<uint64_t> { ++resolved; return 99; }).link();
        check(resolved == 0 && !weak_link.symbol("absent_weak"));
        auto slot = size_t(*weak_link.symbol("slot") - weak_link.base());
        check(slot + 8 <= weak_link.image().size());
        for (size_t i = 0; i < 8; ++i) check(weak_link.image()[slot + i] == 0);
        metald::Linker defined_weak(l);
        defined_weak.define("absent_weak", 99).add("weak.o", weak.bytes()).link();
        slot = size_t(*defined_weak.symbol("slot") - defined_weak.base());
        check(defined_weak.image()[slot] == 99 && defined_weak.symbol("absent_weak") == 99);

        auto weak_definition = moved.compile(".text\n.globl _start\n_start: ret\n.weak replace_me\nreplace_me: ret\n");
        metald::Linker override_weak(l);
        override_weak.define("replace_me", 123).add("definition.o", weak_definition.bytes()).link();
        check(override_weak.symbol("replace_me") == 123);
        std::cout << checks << " C++ embedding checks passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
