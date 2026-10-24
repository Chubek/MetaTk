#!/usr/bin/env python3
"""Real ELF seeds, targeted corruptions, and independent output invariants."""
import os
import pathlib
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
BIN = pathlib.Path(os.environ.get('ARCHLD_BIN', ROOT / 'bin'))
checks = 0


def run(args, expected=0):
    global checks
    p = subprocess.run(list(map(str, args)), capture_output=True, text=True,
                       timeout=20)
    assert p.returncode == expected, (args, p.returncode, p.stdout, p.stderr)
    checks += 1
    return p


class ELF:
    """Independent test-side ELF decoder; never used by the linker."""
    def __init__(self, data):
        self.data = bytearray(data)
        self.is64 = data[4] == 2
        self.word = 'Q' if self.is64 else 'I'
        self.shoff = self.get(40 if self.is64 else 32, self.word)
        self.shsize = self.get(58 if self.is64 else 46, 'H')
        self.shnum = self.get(60 if self.is64 else 48, 'H')
        self.strindex = self.get(62 if self.is64 else 50, 'H')
        self.fields = dict(zip(
            ['name', 'type', 'flags', 'addr', 'offset', 'size', 'link', 'info', 'align', 'entsize'],
            [0, 4, 8, 16, 24, 32, 40, 44, 48, 56] if self.is64 else range(0, 40, 4)))

    def get(self, offset, fmt):
        return struct.unpack_from('<' + fmt, self.data, offset)[0]

    def put(self, offset, fmt, value):
        struct.pack_into('<' + fmt, self.data, offset, value)

    def field(self, index, key, value=None):
        off = self.shoff + index * self.shsize + self.fields[key]
        fmt = 'I' if key in ('name', 'type', 'link', 'info') else self.word
        if value is not None:
            self.put(off, fmt, value)
        return self.get(off, fmt)

    def section(self, name):
        start = self.field(self.strindex, 'offset')
        for i in range(self.shnum):
            off = start + self.field(i, 'name')
            end = self.data.index(0, off)
            if self.data[off:end].decode() == name:
                return i
        raise AssertionError(name)


def archive_member(name, body):
    header = (name.ljust(16) + '0'.ljust(12) + '0'.ljust(6) +
              '0'.ljust(6) + '100644'.ljust(8) + str(len(body)).ljust(10) + '`\n')
    return header.encode() + body + (b'\n' if len(body) % 2 else b'')


with tempfile.TemporaryDirectory(prefix='archld-hardening-') as tmp:
    d = pathlib.Path(tmp)
    seed_source = '''.globl _start, datum, zeroes, common, edge
.text
_start: call helper
edge:
.data
datum: .long 37
.bss
.balign 32
zeroes: .skip 40
.comm common,24,16
'''
    (d / 'seed.s').write_text(seed_source)
    for bits in (64, 32):
        target = 'ELF' + str(bits)
        run(['as', '--' + str(bits), '-o', d / 'seed.o', d / 'seed.s'])
        original = (d / 'seed.o').read_bytes()
        elf = ELF(original)
        sym = elf.section('.symtab')
        strings = elf.field(sym, 'link')
        text = elf.section('.text')
        rel = elf.section('.rela.text' if bits == 64 else '.rel.text')
        mutations = [
            ('byte-order', lambda e: e.put(5, 'B', 0), 'byte order'),
            ('ident-version', lambda e: e.put(6, 'B', 0), 'version'),
            ('header-version', lambda e: e.put(20, 'I', 2), 'version'),
            ('header-size', lambda e: e.put(52 if bits == 64 else 40, 'H', 1), 'header size'),
            ('section-table', lambda e: e.put(40 if bits == 64 else 32, e.word, len(original)), 'outside'),
            ('string-index', lambda e: e.put(62 if bits == 64 else 50, 'H', 65535), 'section name index'),
            ('no-names', lambda e: e.put(62 if bits == 64 else 50, 'H', 0), 'without'),
            ('string-type', lambda e: e.field(e.strindex, 'type', 1), 'SHT_STRTAB'),
            ('string-name', lambda e: e.field(text, 'name', 0xffffffff), 'string table reference'),
            ('string-end', lambda e: e.put(e.field(strings, 'offset') + e.field(strings, 'size') - 1, 'B', 65), 'end with NUL'),
            ('symbol-name', lambda e: e.put(e.field(sym, 'offset') + e.field(sym, 'entsize'), 'I', 0xffffffff), 'string table reference'),
            ('symbol-link', lambda e: e.field(sym, 'link', text), 'SHT_STRTAB'),
            ('symbol-entry', lambda e: e.field(sym, 'entsize', 0), 'entry size'),
            ('symbol-section', lambda e: e.put(e.field(sym, 'offset') + e.field(sym, 'entsize') + (6 if bits == 64 else 14), 'H', 65535), 'symbol section index'),
            ('section-alignment', lambda e: e.field(text, 'align', 3), 'alignment'),
            ('section-payload', lambda e: e.field(text, 'offset', len(original)), 'payload'),
            ('relocation-link', lambda e: e.field(rel, 'link', text), 'relocation symbol table'),
            ('relocation-target', lambda e: e.field(rel, 'info', e.shnum), 'target section'),
            ('relocation-entry', lambda e: e.field(rel, 'entsize', 0), 'entry size'),
            ('relocation-symbol', lambda e: e.put(e.field(rel, 'offset') + (8 if bits == 64 else 4), e.word, (0xffffffff << 32 | 2) if bits == 64 else 0xffffff02), 'symbol index'),
        ]
        for label, mutate, message in mutations:
            damaged = ELF(original)
            mutate(damaged)
            obj = d / (label + '.o')
            obj.write_bytes(damaged.data)
            # Reader checks apply equally to inspection tools and the linker.
            for tool in ('ArchRelocs', 'ArchLD'):
                args = [BIN / tool, '--target', target]
                out = d / 'sentinel'
                out.write_bytes(b'keep-existing-output')
                if tool == 'ArchLD':
                    args += ['-o', out]
                p = run(args + [obj], expected=1)
                assert message in p.stderr, (label, p.stderr)
                assert str(obj) in p.stderr, p.stderr
                assert out.read_bytes() == b'keep-existing-output'

        # These are valid object metadata but unsupported link semantics.
        for label, flags, message in [('wx', 7, 'writable executable'),
                                      ('tls', 0x403, 'unsupported')]:
            damaged = ELF(original)
            damaged.field(text, 'flags', flags)
            (d / 'flags.o').write_bytes(damaged.data)
            p = run([BIN / 'ArchLD', '--target', target, d / 'flags.o'], expected=1)
            assert message in p.stderr, (label, p.stderr)
        damaged = ELF(original)
        damaged.field(damaged.section('.bss'), 'flags', 6)
        (d / 'executable-bss.o').write_bytes(damaged.data)
        p = run([BIN / 'ArchLD', '--target', target, d / 'executable-bss.o'], expected=1)
        assert 'executable NOBITS' in p.stderr, p.stderr
        (d / 'ifunc.s').write_text('.globl helper\n.type helper,@gnu_indirect_function\n.text\nhelper: ret\n')
        run(['as', '--' + str(bits), '-o', d / 'ifunc.o', d / 'ifunc.s'])
        p = run([BIN / 'ArchLD', '--target', target, d / 'seed.o', d / 'ifunc.o'], expected=1)
        assert 'IFUNC symbol helper' in p.stderr, p.stderr

        # A defined helper lets the output metadata and implicit REL path be tested.
        (d / 'helper.s').write_text('.globl helper\n.text\nhelper: ' +
            ('mov $60,%eax; mov $37,%edi; syscall\n' if bits == 64 else
             'mov $1,%eax; mov $37,%ebx; int $0x80\n'))
        run(['as', '--' + str(bits), '-o', d / 'helper.o', d / 'helper.s'])
        run([os.environ.get('ARCHLD_API_TEST', ROOT / 'build/test-api'),
             ROOT / 'targets/ArchBFDCatalogue.json', target, d / 'seed.o', d / 'helper.o'])
        out = d / 'linked'
        run([BIN / 'ArchLD', '--target', target, '-o', out, d / 'seed.o', d / 'helper.o'])
        for args in (['-hW'], ['-lW'], ['-SW'], ['-sW']):
            p = run(['readelf', *args, out])
            assert not p.stderr, p.stderr
        result = ELF(out.read_bytes())
        bss = result.section('.bss')
        assert result.field(bss, 'size') >= 64
        assert result.field(bss, 'type') == 8
        symtab = result.section('.symtab')
        symnames = result.field(result.field(symtab, 'link'), 'offset')
        names = {}
        for off in range(result.field(symtab, 'offset'), result.field(symtab, 'offset') + result.field(symtab, 'size'), result.field(symtab, 'entsize')):
            nameoff = symnames + result.get(off, 'I')
            name = result.data[nameoff:result.data.index(0, nameoff)].decode()
            index = result.get(off + (6 if bits == 64 else 14), 'H')
            value = result.get(off + (8 if bits == 64 else 4), result.word)
            names[name] = (index, value)
        for name in ('zeroes', 'common'):
            index, value = names[name]
            assert index == bss, (name, index, bss)
            assert result.field(bss, 'addr') <= value < result.field(bss, 'addr') + result.field(bss, 'size')
        assert names['datum'][0] == result.section('.data')
        assert names['edge'][0] == result.section('.text')
        if bits == 64:
            run([out], expected=37)
        else:
            # GABI 6.1 defines Elf32_Rela.r_addend as a signed Elf32_Sword.
            # Rewrite the assembler's one REL into an equivalent explicit RELA.
            rela = ELF(original)
            old = rela.field(rel, 'offset')
            patch_offset = rela.get(old, 'I')
            info = rela.get(old + 4, 'I')
            addend = rela.get(rela.field(text, 'offset') + patch_offset, 'i')
            assert addend == -4
            rela.field(rel, 'type', 4)
            rela.field(rel, 'offset', len(rela.data))
            rela.field(rel, 'size', 12)
            rela.field(rel, 'entsize', 12)
            rela.data.extend(struct.pack('<IIi', patch_offset, info, addend))
            (d / 'rela32.o').write_bytes(rela.data)
            p = run(['readelf', '-rW', d / 'rela32.o'])
            assert 'helper - 4' in p.stdout, p.stdout
            p = run([BIN / 'ArchRelocs', '--target', target, d / 'rela32.o'])
            assert 'addend=-4' in p.stdout, p.stdout
            run([BIN / 'ArchLD', '--target', target, '-o', d / 'rela32', d / 'rela32.o', d / 'helper.o'])
            assert (d / 'rela32').read_bytes() == out.read_bytes()

        # Relocation diagnostics must retain input, section, symbol, and reason.
        damaged = ELF(original)
        offset = damaged.field(rel, 'offset') + (8 if bits == 64 else 4)
        info = damaged.get(offset, damaged.word)
        mask = 0xffffffff if bits == 64 else 0xff
        damaged.put(offset, damaged.word, (info & ~mask) | mask)
        (d / 'unsupported.o').write_bytes(damaged.data)
        p = run([BIN / 'ArchLD', '--target', target, d / 'unsupported.o', d / 'helper.o'], expected=1)
        assert 'unsupported.o:.text+' in p.stderr and 'helper' in p.stderr, p.stderr

    # Archive structural checks, including errors in otherwise unneeded members.
    run(['as', '--64', '-o', d / 'seed.o', d / 'seed.s'])
    original = (d / 'seed.o').read_bytes()
    bad = ELF(original)
    bad.put(5, 'B', 0)
    archives = [
        ('malformed-member', archive_member('broken.o/', bad.data), 'broken.o'),
        ('non-object', archive_member('text/', b'not ELF'), 'non-ELF'),
        ('gnu-unterminated', archive_member('//', b'long-name') + archive_member('/0', original), 'unterminated GNU'),
        ('gnu-interior', archive_member('//', b'long-name/\n') + archive_member('/1', original), 'entry boundary'),
        ('gnu-duplicate', archive_member('//', b'name/\n') * 2, 'duplicate GNU'),
        ('bsd-embedded-nul', archive_member('#1/3', b'a\0b' + original), 'embedded NUL'),
        ('bsd-empty', archive_member('#1/0', original), 'empty archive member name'),
        ('padding', archive_member('odd/', b'123')[:-1], 'padding'),
        ('bad-padding', archive_member('odd/', b'123')[:-1] + b'X', 'padding'),
    ]
    for label, members, message in archives:
        path = d / (label + '.a')
        path.write_bytes(b'!<arch>\n' + members)
        p = run([BIN / 'ArchLD', d / 'seed.o', path], expected=1)
        assert message in p.stderr and str(path) in p.stderr, p.stderr

    # GNU long names and BSD extended names remain usable.
    (d / 'helper.s').write_text('.globl helper\n.text\nhelper: mov $60,%eax; mov $37,%edi; syscall\n')
    run(['as', '--64', '-o', d / 'a-very-long-helper-name.o', d / 'helper.s'])
    run(['ar', 'rcs', d / 'long.a', d / 'a-very-long-helper-name.o'])
    member = (d / 'a-very-long-helper-name.o').read_bytes()
    name = b'a-long-bsd-helper-name.o\0\0'
    (d / 'bsd.a').write_bytes(b'!<arch>\n' + archive_member('#1/' + str(len(name)), name + member))
    for name in ('long.a', 'bsd.a'):
        run([BIN / 'ArchLD', '-o', d / 'archive-linked', d / 'seed.o', d / name])
        run([d / 'archive-linked'], expected=37)

print(f'PASS: {checks} hardening/metadata commands (ELF32, ELF64, GNU/BSD archives)')
