"""Tests for the object-format (ArchBFD) Python bindings."""
import os
import unittest

from metaarch import archbfd
from metaarch.archbfd import (ArchBFDCatalogue, ArchBFDError, ArchBFDObject,
                               ArchBFDSpec, Format)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DOCUMENTS = os.path.join(ROOT, "ArchBFD")
CATALOGUE = os.path.join(ROOT, "ArchBFDCatalogue.yaml")


class DocumentTests(unittest.TestCase):
    def test_load_document(self):
        spec = archbfd.load(os.path.join(DOCUMENTS, "ELF64.bfd.yaml"))
        self.assertEqual(spec.name, "ELF64")
        self.assertEqual(spec.format, "elf")
        self.assertEqual(spec.byte_order, "little")
        self.assertEqual(spec.target_vector, "elf64-x86-64")
        self.assertEqual(spec.arch, "x86-64")
        self.assertEqual(spec.magic, "7f454c46")
        self.assertEqual(spec.schema, 1)
        self.assertEqual(spec.record_count, 6)
        self.assertGreater(spec.reloc_count, 30)
        self.assertGreater(spec.constant_group_count, 0)
        self.assertGreater(spec.source_count, 0)
        self.assertIn("bfd/", spec.source(0).file)

        record = spec.record(0)
        self.assertEqual(record.name, "ehdr")
        self.assertEqual(record.size, 64)
        self.assertEqual(len(record.fields), 14)
        self.assertEqual(record.fields[0].name, "e_ident")
        self.assertEqual(record.fields[0].size, 16)

        index = spec.find_reloc("R_X86_64_PC32")
        self.assertGreaterEqual(index, 0)
        reloc = spec.relocation(index)
        self.assertEqual(reloc.number, 2)
        self.assertEqual(reloc.size, 4)
        self.assertEqual(reloc.bitsize, 32)
        self.assertTrue(reloc.pcrel)
        self.assertEqual(reloc.overflow, archbfd.Overflow.SIGNED)
        self.assertEqual(spec.find_reloc("R_X86_64_NOPE"), -1)
        self.assertGreaterEqual(spec.find_reloc_number(1), 0)

        self.assertEqual(spec.constant("section_types", "SHT_RELA"), 4)
        self.assertIsNone(spec.constant("section_types", "SHT_NOPE"))
        self.assertIsNotNone(spec.find_record("shdr"))
        self.assertIsNone(spec.find_record("nope"))

    def test_write_round_trip(self):
        spec = archbfd.load(os.path.join(DOCUMENTS, "ELF32.bfd.yaml"))
        text = spec.write(archbfd.Format.AUTO if False else 1)  # JSON
        copy = archbfd.load_buffer(text, 1)
        self.assertEqual(copy.name, "ELF32")
        self.assertEqual(copy.reloc_count, spec.reloc_count)

    def test_invalid_document(self):
        with self.assertRaises(ArchBFDError) as raised:
            archbfd.load_buffer("schema: 3\n", archbfd.DocumentFormat.YAML, "bad.yaml")
        self.assertTrue(raised.exception.diagnostics)

    def test_embedded_nul(self):
        with self.assertRaises(ValueError):
            archbfd.load("bad\x00path")


class CatalogueTests(unittest.TestCase):
    def test_catalogue(self):
        with ArchBFDCatalogue.open(CATALOGUE) as db:
            self.assertGreaterEqual(db.count, 8)
            self.assertEqual(db.entry_name(0), "COFF")
            self.assertTrue(db.entry_path(0).endswith("COFF.bfd.yaml"))
            self.assertEqual(db.entry_format(0), "coff")
            self.assertEqual(db.entry_target_vector(0), "coff-i386")

            spec = db.load("ELF64")
            self.assertEqual(spec.name, "ELF64")
            self.assertEqual(spec.target_vector, "elf64-x86-64")

    def test_scan(self):
        with ArchBFDCatalogue.scan(DOCUMENTS) as db:
            self.assertGreaterEqual(db.count, 8)
            spec = db.load("m68k")
            self.assertEqual(spec.container, "ELF32")
            self.assertEqual(spec.byte_order, "big")

    def test_unknown_document(self):
        with ArchBFDCatalogue.open(CATALOGUE) as db:
            with self.assertRaises(ArchBFDError):
                db.load("no-such-format")


class ObjectTests(unittest.TestCase):
    OBJECT = os.path.join(ROOT, "build", "archbfd-test.o")

    def test_read_object(self):
        if not os.path.exists(self.OBJECT):
            self.skipTest("no test object")
        with ArchBFDCatalogue.open(CATALOGUE) as db:
            spec = db.load("ELF64")
            with ArchBFDObject.open(self.OBJECT, spec) as obj:
                self.assertEqual(obj.kind, "elf64")
                self.assertEqual(obj.machine, 62)
                self.assertEqual(obj.type, 1)
                self.assertEqual(obj.section_count, 6)
                self.assertEqual(obj.symbol_count, 3)
                self.assertEqual(obj.relocation_count, 1)
                section = obj.section(1)
                self.assertEqual(section.name, ".text")
                self.assertEqual(section.size, 16)
                symbol = obj.symbol(1)
                self.assertEqual(symbol.name, "g")
                self.assertEqual(symbol.bind, "STB_GLOBAL")
                reloc = obj.relocation(0)
                self.assertEqual(reloc.type_name, "R_X86_64_PC32")
                self.assertEqual(reloc.symbol_name, "g")
                self.assertEqual(reloc.offset, 8)
                self.assertEqual(reloc.addend, -4)
                with self.assertRaises(IndexError):
                    obj.section(99)


if __name__ == "__main__":
    unittest.main()
