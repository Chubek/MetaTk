-- Executable ASMtk subset of amd64.dsymspec. This is the single source for
-- the dependency-free C++ tables and the loadable Symspec JSON document.
local M = {}
M.gpr64 = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi" }
M.gpr32 = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" }
M.gpr16 = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" }
M.gpr8 = { "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil" }

local registers, encodings = {}, {}
for code = 0, 15 do
    for _, width in ipairs({8, 32, 64}) do
        local names = M["gpr" .. width]
        local suffix = width == 8 and "b" or width == 32 and "d" or ""
        registers[#registers + 1] = {
            name = code < 8 and names[code + 1] or "r" .. code .. suffix,
            class = "gpr", width = width, enc = code,
        }
    end
end
local function add(mnemonic, form, widths, opcode, group, reg_operand, rex_w, relocation)
    encodings[#encodings + 1] = {
        mnemonic = mnemonic, form = form, widths = widths, opcode = opcode,
        group = group or 0, reg_operand = reg_operand or 1,
        rex_w = rex_w ~= false, relocation = relocation or 0,
        implicit_register = form == "ShiftCL" and "cl" or "",
    }
end
for _, fixed in ipairs({
    {"nop", {0x90}}, {"ret", {0xc3}}, {"syscall", {0x0f, 0x05}},
    {"int3", {0xcc}}, {"ud2", {0x0f, 0x0b}}, {"cqo", {0x48, 0x99}},
    {"leave", {0xc9}},
}) do
    add(fixed[1], "Fixed", {}, fixed[2])
end
for _, branch in ipairs({{"jmp", 0xe9, 4, 2}, {"call", 0xe8, 2, 4}}) do
    add(branch[1], "Relative", {}, {branch[2]}, 0, 1, false, branch[4])
    add(branch[1], "Indirect", {64}, {0xff}, branch[3], 1, false)
end
for code, aliases in ipairs({
    {"jo"}, {"jno"}, {"jb", "jc"}, {"jae", "jnc"}, {"je", "jz"},
    {"jne", "jnz"}, {"jbe"}, {"ja"}, {"js"}, {"jns"}, {"jp"},
    {"jnp"}, {"jl"}, {"jge"}, {"jle"}, {"jg"},
}) do
    for _, mnemonic in ipairs(aliases) do
        add(mnemonic, "Relative", {}, {0x0f, 0x80 + code - 1}, 0, 1, false, 2)
    end
end
add("push", "OpcodeRegister", {64}, {0x50}, 0, 1, false)
add("pop", "OpcodeRegister", {64}, {0x58}, 0, 1, false)
for _, unary in ipairs({
    {"inc", 0xff, 0}, {"dec", 0xff, 1}, {"not", 0xf7, 2},
    {"neg", 0xf7, 3}, {"div", 0xf7, 6}, {"idiv", 0xf7, 7},
}) do
    add(unary[1], "Unary", {32, 64}, {unary[2]}, unary[3])
end
for _, width in ipairs({8, 32, 64}) do
    local byte = width == 8
    add("mov", "RegisterRegister", {width}, {byte and 0x88 or 0x89})
    add("mov", "Load", {width}, {byte and 0x8a or 0x8b}, 0, 0)
    add("mov", "Store", {width}, {byte and 0x88 or 0x89})
    add("mov", "MoveImmediate", {width}, {byte and 0xb0 or 0xb8})
end
add("lea", "Load", {32, 64}, {0x8d}, 0, 0)
for _, binary in ipairs({
    {"add", 0x01, 0}, {"or", 0x09, 1}, {"and", 0x21, 4},
    {"sub", 0x29, 5}, {"xor", 0x31, 6}, {"cmp", 0x39, 7},
}) do
    add(binary[1], "RegisterRegister", {32, 64}, {binary[2]})
    add(binary[1], "ArithmeticImmediate", {32, 64}, {0x81}, binary[3])
end
add("test", "RegisterRegister", {32, 64}, {0x85})
add("imul", "RegisterRegister", {32, 64}, {0x0f, 0xaf}, 0, 0)
for _, shift in ipairs({{"shl", 4}, {"shr", 5}, {"sar", 7}}) do
    add(shift[1], "ShiftCL", {32, 64}, {0xd3}, shift[2])
    add(shift[1], "ShiftImmediate", {32, 64}, {0xc1}, shift[2])
end

M.document = {
    schema = 1,
    targets = {{arch = "amd64", subarch = "x86-64", addr_size = 64, byte_order = "little"}},
    registers = registers,
    asmtk = {schema = 1, encodings = encodings},
}

-- Legacy DomSymAsCC encoder programs for the representative forms already
-- exported by amd64.dsymspec. Derive their opcode bytes from the same bank.
function M.source(mnemonic, form, width)
    for _, e in ipairs(encodings) do
        local matches = #e.widths == 0
        for _, w in ipairs(e.widths) do matches = matches or w == width end
        if e.mnemonic == mnemonic and e.form == form and matches then
            local out = {}
            local function emit(expression) out[#out + 1] = "emit8(" .. expression .. ");" end
            if e.form ~= "Fixed" and e.form ~= "Relative" then
                local r, b = "0", "dst"
                if e.form == "RegisterRegister" then
                    r = e.reg_operand == 0 and "dst" or "src"
                    b = e.reg_operand == 0 and "src" or "dst"
                end
                local rex = string.format("(0x%X | ((%s >> 3) << 2) | (%s >> 3))",
                    e.rex_w and width == 64 and 0x48 or 0x40, r, b)
                out[#out + 1] = "if (" .. rex .. " != 0x40) { emit8(" .. rex .. "); }"
            end
            for i, byte in ipairs(e.opcode) do
                emit(string.format("0x%02X", byte) ..
                    ((e.form == "MoveImmediate" or e.form == "OpcodeRegister") and i == #e.opcode and " + (dst & 7)" or ""))
            end
            if e.form == "RegisterRegister" then
                out[#out + 1] = e.reg_operand == 0 and "emit_modrm(dst, src);" or "emit_modrm(src, dst);"
            elseif e.form == "Unary" or e.form == "ShiftCL" then
                out[#out + 1] = "emit_modrm(" .. e.group .. ", dst);"
            elseif e.form == "MoveImmediate" then
                out[#out + 1] = "emit_imm(" .. math.floor(width / 8) .. ", imm);"
            elseif e.form == "Relative" then
                out[#out + 1] = "emit_imm(4, 0); emit_relocation(sym, " ..
                    (e.relocation == 4 and "R_X86_64_PLT32" or "R_X86_64_PC32") .. ");"
            end
            return table.concat(out, " ")
        end
    end
    error("missing ASMtk encoding: " .. mnemonic .. "/" .. form)
end
return M
