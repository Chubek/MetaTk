// Tests for the standalone C++17 ArchBFD API (ArchBFD.hpp).
#include "ArchBFD.hpp"

#include <iostream>
#include <string>
#include <type_traits>

using archbfd::api::Catalogue;
using archbfd::api::Object;
using archbfd::api::Spec;
using archbfd::api::SpecView;

static_assert(!std::is_copy_constructible<Spec>::value, "spec must not double-free");
static_assert(std::is_nothrow_move_constructible<Spec>::value, "movable RAII handle");
static_assert(!std::is_copy_constructible<Catalogue>::value, "catalogue must not double-free");
static_assert(!std::is_copy_constructible<Object>::value, "object must not double-free");

static int checks, failures;

#define CHECK(cond)                                                             \
    do {                                                                         \
        checks++;                                                                \
        if (!(cond)) {                                                           \
            failures++;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " << #cond   \
                      << "\n";                                                   \
        }                                                                        \
    } while (0)

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "usage: test_archbfd_cpp ARCHBFD_DIR CATALOGUE\n";
        return 2;
    }
    const std::string dir = argv[1];
    const std::string catalogue = argv[2];
    try {
        // --- a document loaded on its own ---------------------------------
        Spec spec = Spec::load(dir + "/ELF64.bfd.yaml");
        CHECK(spec.name() == "ELF64");
        CHECK(spec.format() == "elf");
        CHECK(spec.byte_order() == "little");
        CHECK(spec.target_vector() == "elf64-x86-64");
        CHECK(spec.magic() == "7f454c46");
        CHECK(spec.schema() == 1);
        CHECK(spec.record_count() == 6);
        CHECK(spec.reloc_count() > 30);
        CHECK(spec.constant_group_count() == 8);
        CHECK(spec.source_count() > 0);
        CHECK(spec.source(0).file.find("bfd/") != std::string::npos);

        // The records carry the binutils layout.
        bool have_ehdr = false;
        for (const auto &r : spec.records()) {
            if (r.name != "ehdr") continue;
            have_ehdr = true;
            CHECK(r.size == 64);
            CHECK(r.fields.size() == 14);
            CHECK(r.fields[0].name == "e_ident" && r.fields[0].size == 16);
            CHECK(r.fields[0].offset == 0);
        }
        CHECK(have_ehdr);

        // The relocations carry the numbers and the policies.
        int32_t pc32 = spec.find_reloc("R_X86_64_PC32");
        CHECK(pc32 >= 0);
        if (pc32 >= 0) {
            const auto r = spec.relocation(static_cast<size_t>(pc32));
            CHECK(r.number == 2);
            CHECK(r.size == 4);
            CHECK(r.bitsize == 32);
            CHECK(r.pcrel);
        }
        CHECK(spec.find_reloc_number(1) >= 0);
        CHECK(spec.find_reloc("no-such-reloc") == -1);

        int64_t value = 0;
        CHECK(spec.constant("section_types", "SHT_RELA", &value) && value == 4);
        CHECK(!spec.constant("section_types", "SHT_NOPE", &value));

        archbfd::api::Record record;
        CHECK(spec.find_record("shdr", &record));
        CHECK(record.size == 64 && record.fields.size() == 10);
        CHECK(!spec.find_record("nope", &record));

        // --- writing ------------------------------------------------------
        const std::string json = spec.write(DSYM_FORMAT_JSON);
        CHECK(!json.empty());
        Spec copy = Spec::load_buffer(json, DSYM_FORMAT_JSON, "copy.json");
        CHECK(copy.name() == "ELF64");
        CHECK(copy.reloc_count() == spec.reloc_count());

        // --- the catalogue ------------------------------------------------
        Catalogue db = Catalogue::open(catalogue);
        CHECK(db.count() >= 8);
        CHECK(db.entry_name(0).size() > 0);
        SpecView elf64 = db.load("ELF64");
        CHECK(elf64.name() == "ELF64");
        CHECK(elf64.find_reloc("R_X86_64_PC32") == pc32);
        SpecView by_vector = db.load("elf64-x86-64");
        CHECK(by_vector.name() == "ELF64");

        // --- the object reader --------------------------------------------
        // The catalogue document is read through the same description.
        CHECK(elf64.record_count() == 6);

        // --- error handling ------------------------------------------------
        bool failed = false;
        try {
            Spec::load(dir + "/does-not-exist.yaml");
        } catch (const archbfd::api::Error &e) {
            failed = !e.diagnostics().empty();
        }
        CHECK(failed);
        failed = false;
        try {
            Spec::load_buffer("schema: 3\n", DSYM_FORMAT_YAML, "bad.yaml");
        } catch (const archbfd::api::Error &e) {
            failed = !e.diagnostics().empty();
        }
        CHECK(failed);
        failed = false;
        try {
            Spec::load(std::string("bad\0path", 8));
        } catch (const std::invalid_argument &) {
            failed = true;
        }
        CHECK(failed);
        failed = false;
        try {
            db.load("no-such-format");
        } catch (const archbfd::api::Error &) {
            failed = true;
        }
        CHECK(failed);
    } catch (const std::exception &e) {
        std::cerr << "unexpected exception: " << e.what() << "\n";
        return 1;
    }
    std::cout << checks << " C++ API check(s), " << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
