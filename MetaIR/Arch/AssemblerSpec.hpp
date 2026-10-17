#pragma once

#include "Encoding.hpp"
#include "Symspec.hpp"
#include <map>

namespace symspec::encoding {
struct Tables {
    // Views borrow the immutable Spec, just like Symspec's other query results.
    std::vector<Register> registers;
    std::vector<Encoding> encodings;
};
inline Result<Tables> assembler_tables(const Spec& spec) {
    const auto arch = spec.architecture();
    if (arch != "amd64" && arch != "x86_64" && arch != "x86-64")
        return fail(ErrorCode::unsupported, "ASMtk supports x86-64 only");
    const auto* target = spec.target();
    if (!target || integer(field(*target, "addr_size")) != 64 ||
        (text(field(*target, "byte_order")) != "little" && text(field(*target, "byte_order")) != "le"))
        return fail(ErrorCode::schema, "ASMtk requires a 64-bit little-endian target");
    const auto* extension = field(spec.json(), "asmtk");
    if (!extension || integer(field(*extension, "schema")) != 1)
        return fail(ErrorCode::schema, "architecture requires an asmtk schema 1 encoding bank");
    const auto* registers = spec.assembler("registers");
    const auto* encodings = field(*extension, "encodings");
    if (!registers || !encodings || !encodings->is_array())
        return fail(ErrorCode::schema, "architecture requires registers and asmtk.encodings arrays");
    Tables result;
    std::set<std::string_view> names;
    const auto valid_name = [](std::string_view name) {
        return !name.empty() && !(name.front() >= '0' && name.front() <= '9') && std::all_of(name.begin(), name.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        });
    };
    for (const auto& value : *registers) {
        if (text(field(value, "class")) != "gpr") continue;
        auto name = text(field(value, "name"));
        auto code = integer(field(value, "enc")), width = integer(field(value, "width"));
        if (!name || !valid_name(*name) || !code || *code < 0 || !width ||
            (*width != 8 && *width != 16 && *width != 32 && *width != 64))
            return fail(ErrorCode::schema, "invalid ASMtk register record");
        // The broader architecture bank also describes high bytes, 16-bit and
        // APX registers. They are not encodable by this bounded REX backend.
        if (*code > 15 || (*width != 8 && *width != 32 && *width != 64)) continue;
        if (auto* low = field(value, "sub_lo")) {
            auto bit = integer(low);
            if (!bit || *bit < 0) return fail(ErrorCode::schema, "invalid register sub_lo");
            if (*bit != 0) continue;
        }
        if (!names.insert(*name).second) return fail(ErrorCode::schema, "duplicate ASMtk register");
        result.registers.push_back({*name, static_cast<unsigned>(*code), static_cast<unsigned>(*width)});
    }
    constexpr std::pair<std::string_view, Form> forms[] = {
        {"Fixed", Form::Fixed}, {"Relative", Form::Relative}, {"Indirect", Form::Indirect},
        {"OpcodeRegister", Form::OpcodeRegister}, {"Unary", Form::Unary},
        {"RegisterRegister", Form::RegisterRegister}, {"Load", Form::Load}, {"Store", Form::Store},
        {"MoveImmediate", Form::MoveImmediate}, {"ShiftCL", Form::ShiftCL},
        {"ShiftImmediate", Form::ShiftImmediate}, {"ArithmeticImmediate", Form::ArithmeticImmediate},
    };
    std::map<std::pair<std::string_view, Form>, unsigned> signatures;
    for (const auto& value : encodings->as_array()) {
        auto name = text(field(value, "mnemonic")), kind = text(field(value, "form"));
        auto found = std::find_if(std::begin(forms), std::end(forms), [&](auto f) { return kind == f.first; });
        if (!name || !valid_name(*name) || found == std::end(forms))
            return fail(ErrorCode::schema, "invalid ASMtk mnemonic or encoding form");
        Encoding e{*name, found->second, 0, {}, 0, 0, 1, true, 0, {}};
        const auto* widths = field(value, "widths");
        const auto* opcode = field(value, "opcode");
        if (!widths || !widths->is_array() || !opcode || !opcode->is_array() ||
            opcode->as_array().empty() || opcode->as_array().size() > e.opcode.size())
            return fail(ErrorCode::schema, "invalid ASMtk widths or opcode array");
        for (const auto& w : widths->as_array()) {
            auto n = integer(&w);
            const unsigned mask = n && (*n == 8 || *n == 32 || *n == 64) ? width_mask(static_cast<unsigned>(*n)) : 0;
            if (!mask || (e.widths & mask)) return fail(ErrorCode::schema, "invalid ASMtk operand width");
            e.widths |= mask;
        }
        for (const auto& byte : opcode->as_array()) {
            auto n = integer(&byte);
            if (!n || *n < 0 || *n > 255) return fail(ErrorCode::schema, "invalid ASMtk opcode byte");
            e.opcode[e.opcode_size++] = static_cast<std::uint8_t>(*n);
        }
        const auto number = [&](std::string_view key, unsigned& target, unsigned maximum) {
            const auto* v = field(value, key);
            if (!v) return true;
            auto n = integer(v);
            if (!n || *n < 0 || static_cast<std::uint64_t>(*n) > maximum) return false;
            target = static_cast<unsigned>(*n); return true;
        };
        if (!number("group", e.group, 7) || !number("reg_operand", e.reg_operand, 1) ||
            !number("relocation", e.relocation, 4))
            return fail(ErrorCode::schema, "invalid ASMtk encoding field");
        if (const auto* w = field(value, "rex_w")) {
            if (!w->is_bool()) return fail(ErrorCode::schema, "ASMtk rex_w must be boolean");
            e.rex_w = w->as_bool();
        }
        if (const auto* implicit = field(value, "implicit_register")) {
            auto n = text(implicit);
            if (!n) return fail(ErrorCode::schema, "invalid ASMtk implicit_register");
            e.implicit_register = *n;
        }
        if (e.form == Form::ShiftCL) {
            if (std::none_of(result.registers.begin(), result.registers.end(), [&](auto r) {
                    return r.name == e.implicit_register && r.width == 8;
                })) return fail(ErrorCode::schema, "ASMtk shift requires an 8-bit implicit register");
        } else if (!e.implicit_register.empty()) {
            return fail(ErrorCode::schema, "unexpected ASMtk implicit register");
        }
        const bool no_width = e.form == Form::Fixed || e.form == Form::Relative;
        if (no_width != (e.widths == 0) ||
            (e.form == Form::Relative ? e.relocation != 2 && e.relocation != 4 : e.relocation != 0) ||
            ((e.form == Form::Indirect || e.form == Form::OpcodeRegister) && e.widths != width_mask(64)) ||
            ((e.form == Form::Unary || e.form == Form::ShiftCL || e.form == Form::ShiftImmediate ||
              e.form == Form::ArithmeticImmediate) && (e.widths & width_mask(8))) ||
            ((e.form == Form::OpcodeRegister || e.form == Form::MoveImmediate) &&
             (e.opcode[e.opcode_size - 1] & 7)))
            return fail(ErrorCode::schema, "inconsistent ASMtk encoding form");
        auto& previous = signatures[{e.mnemonic, e.form}];
        const unsigned signature = no_width ? 8u : e.widths;
        if (previous & signature) return fail(ErrorCode::schema, "duplicate ASMtk encoding form");
        previous |= signature;
        result.encodings.push_back(e);
    }
    if (result.registers.empty() || result.encodings.empty())
        return fail(ErrorCode::schema, "ASMtk architecture bank must not be empty");
    return result;
}
} // namespace symspec::encoding
