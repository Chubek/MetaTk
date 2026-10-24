"""Offline tests using synthetic fixtures plus actual architecture specs."""
import gc
import json
import os
from pathlib import Path
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor

import metaarch as ma

ROOT = Path(os.environ.get("METAARCH_SOURCE_DIR", Path(__file__).resolve().parents[3]))
FIXTURE = ROOT / "tests/fixtures/api.yaml"


class BindingsTests(unittest.TestCase):
    def setUp(self):
        self.spec = ma.load(FIXTURE)

    def tearDown(self):
        self.spec.close()

    def test_version(self):
        self.assertEqual(ma.abi_version(), 1)
        self.assertEqual(ma.__version__, "1.0.0")

    def test_metadata(self):
        self.assertEqual((self.spec.target_count, self.spec.register_count,
                          self.spec.encoder_count, self.spec.form_count), (2, 2, 3, 3))
        target = self.spec.target(0)
        self.assertEqual((target.arch, target.subarch, target.object_format,
                          target.abi, target.address_width, target.byte_order),
                         ("test", "v1", "ELF32", "test", 32, ma.ByteOrder.LITTLE))
        self.assertEqual(self.spec.target(1).byte_order, ma.ByteOrder.BIG)
        self.assertEqual(self.spec.register("r1").encoding, 1)
        self.assertEqual(self.spec.register(0).reg_class, "gpr")
        self.assertEqual(self.spec.register(0).width, 32)
        self.assertEqual(self.spec.form(0).visible_operand_count, 2)
        self.assertEqual(self.spec.form(0).operand_count, 3)
        self.assertEqual(self.spec.form(0).mnemonic, "pair")
        self.assertEqual(self.spec.encoder_name(0), "pair")

    def test_lookup(self):
        self.assertEqual(self.spec.find_encoder("ret"), 2)
        self.assertEqual(self.spec.find_register("r1"), 1)
        self.assertEqual(self.spec.find_encoder("missing"), -1)
        self.assertEqual(self.spec.find_register("missing"), -1)
        with self.assertRaises(IndexError):
            self.spec.register("missing")

    def test_named_encode(self):
        result = self.spec.encode("pair", {"imm": -1, "dst": 1})
        self.assertEqual(result.data, bytes.fromhex("01 ff 00 00"))
        self.assertEqual(result.relocations, [])
        self.assertEqual(self.spec.encode("ret", {}).data, b"\xc3")

    def test_target_endianness(self):
        self.assertEqual(self.spec.encode("pair", {"dst": 1, "imm": -1}, target=1).data,
                         bytes.fromhex("00 00 ff 01"))

    def test_checked_form(self):
        self.assertEqual(self.spec.encode_form(0, [1, -1]).data, bytes.fromhex("01 ff 00 00"))
        for values in ([1, 11], [2, 0], [1], [1, 0, 1]):
            with self.subTest(values=values), self.assertRaises(ma.Error):
                self.spec.encode_form(0, values)
        # Low-level encoding deliberately does not enforce the form's [-10,10] range.
        self.spec.encode("pair", {"dst": 1, "imm": 11})

    def test_relocations_survive_close(self):
        result = self.spec.encode("branch", {"symbol": -7})
        self.spec.close()
        gc.collect()
        self.assertEqual(result.data, bytes.fromhex("aa a0 00 00 05"))
        r, = result.relocations
        self.assertEqual((r.name, r.offset, r.size, r.bit_offset, r.bit_width,
                          r.byte_order, r.symbol_parameter, r.symbol_value),
                         ("R_FIX", 1, 4, 5, 20, ma.ByteOrder.BIG, 0, -7))

    def test_metadata_survives_close(self):
        target, reg, form = self.spec.target(0), self.spec.register(1), self.spec.form(0)
        self.spec.close()
        self.assertEqual((target.arch, reg.name, form.encoder), ("test", "r1", "pair"))
        with self.assertRaises(AttributeError):
            reg.name = "changed"

    def test_roundtrip_every_format(self):
        for fmt in (ma.Format.YAML, ma.Format.JSON, ma.Format.XML, ma.Format.SEXPR):
            for flags in (ma.WriteFlags.NONE, ma.WriteFlags.COMPACT):
                with self.subTest(format=fmt, flags=flags):
                    text = self.spec.write(fmt, flags | ma.WriteFlags.REF_BACKEND)
                    with ma.parse(text, fmt) as other:
                        self.assertEqual(other.register_count, 2)
                        self.assertEqual(other.encode_form(0, [1, -1]).data,
                                         self.spec.encode_form(0, [1, -1]).data)

    def test_parse_bytes_and_nul_length(self):
        with ma.parse(FIXTURE.read_bytes(), ma.Format.YAML) as spec:
            self.assertEqual(spec.encoder_count, 3)
        with self.assertRaises(ma.Error):
            ma.parse(b'{"schema":1}\x00garbage', ma.Format.JSON)
        with self.assertRaises(TypeError):
            ma.parse(123, ma.Format.JSON)

    def test_positioned_diagnostics(self):
        with self.assertRaises(ma.Error) as caught:
            ma.parse("schema: 1\ntargets: wrong\n", ma.Format.YAML, path="bad.yaml")
        diagnostics = caught.exception.diagnostics
        self.assertTrue(diagnostics)
        self.assertEqual(diagnostics[-1].file, "bad.yaml")
        self.assertEqual(diagnostics[-1].line, 2)
        self.assertEqual(diagnostics[-1].severity, ma.Severity.ERROR)
        self.assertIn("targets", diagnostics[-1].path)
        self.assertTrue(diagnostics[-1].message)

    def test_missing_file(self):
        with self.assertRaises(ma.Error) as caught:
            ma.load(ROOT / "tests/does-not-exist.yaml")
        self.assertTrue(caught.exception.diagnostics)

    def test_bad_arguments(self):
        for encoder, operands in (("missing", {}), ("pair", {"dst": 1}),
                                  ("pair", {"dst": 1, "imm": 0, "other": 1})):
            with self.subTest(encoder=encoder, operands=operands), self.assertRaises(ma.Error):
                self.spec.encode(encoder, operands)
        with self.assertRaises(ma.Error):
            self.spec.encode("ret", {}, target=99)
        with self.assertRaises(ma.Error):
            self.spec.encode_form(99, [])

    def test_integer_type_and_overflow(self):
        for value in (1.5, "r1", True):
            with self.subTest(value=value), self.assertRaises(TypeError):
                self.spec.encode("pair", {"dst": value, "imm": 0})
            with self.subTest(form_value=value), self.assertRaises(TypeError):
                self.spec.encode_form(0, [value, 0])
        for value in (-(1 << 63) - 1, 1 << 63):
            with self.subTest(value=value), self.assertRaises(OverflowError):
                self.spec.encode("pair", {"dst": 1, "imm": value})
            with self.subTest(form_value=value), self.assertRaises(OverflowError):
                self.spec.encode_form(0, [1, value])
        for value in (-(1 << 63), (1 << 63) - 1):
            r, = self.spec.encode("branch", {"symbol": value}).relocations
            self.assertEqual(r.symbol_value, value)
        with self.assertRaises(TypeError):
            self.spec.encode("ret", {1: 1})

    def test_nul_rejected(self):
        calls = (lambda: ma.load("bad\x00.yaml"),
                 lambda: ma.parse("", ma.Format.JSON, path="bad\x00.json"),
                 lambda: self.spec.encode("ret\x00", {}),
                 lambda: self.spec.encode("pair", {"dst\x00": 1, "imm": 0}),
                 lambda: self.spec.register("r0\x00"),
                 lambda: self.spec.find_encoder("ret\x00"),
                 lambda: self.spec.find_register("r0\x00"))
        for call in calls:
            with self.assertRaises(ValueError):
                call()

    def test_bad_indices(self):
        for getter in (self.spec.target, self.spec.register, self.spec.form, self.spec.encoder_name):
            with self.assertRaises(IndexError):
                getter(9999)

    def test_close_and_context_manager(self):
        self.spec.close()
        self.spec.close()
        with self.assertRaises(RuntimeError):
            self.spec.target_count
        with self.assertRaises(RuntimeError):
            self.spec.__enter__()
        with ma.load(FIXTURE) as other:
            self.assertEqual(other.form_count, 3)
        with self.assertRaises(RuntimeError):
            other.write(ma.Format.JSON)

    def test_exception_not_swallowed(self):
        with self.assertRaisesRegex(ValueError, "sentinel"):
            with ma.load(FIXTURE):
                raise ValueError("sentinel")

    def test_no_compile(self):
        with ma.load(FIXTURE, flags=ma.LoadFlags.NO_COMPILE) as spec:
            self.assertEqual(spec.encoder_count, 3)
            with self.assertRaises(ma.Error):
                spec.encode("ret", {})

    def test_backend_load_and_embed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "spec.yaml"
            (Path(directory) / "backend").mkdir()
            (Path(directory) / "backend/test.json").write_text('{"hello":[1,2,3]}', encoding="utf-8")
            path.write_text(FIXTURE.read_text() + '\nbackend: {file: backend/test.json}\n', encoding="utf-8")
            with ma.load(path) as spec:
                spec.load_backend()
                text = spec.write(ma.Format.JSON, ma.WriteFlags.EMBED_BACKEND)
                self.assertEqual(json.loads(text)["backend"], {"hello": [1, 2, 3]})
            with ma.load(path, flags=ma.LoadFlags.BACKEND) as spec:
                self.assertEqual(json.loads(spec.write(ma.Format.JSON))["backend"]["hello"], [1, 2, 3])

    def test_load_warnings(self):
        data = FIXTURE.read_text().replace("imm + 256", "imm + 128")
        with ma.parse(data, ma.Format.YAML, flags=ma.LoadFlags.LINT) as spec:
            warnings = spec.diagnostics
            self.assertTrue(any(d.severity == ma.Severity.WARNING for d in warnings))
        self.assertTrue(warnings[0].message)
        with self.assertRaises(ma.Error):
            ma.parse(data, ma.Format.YAML, flags=ma.LoadFlags.STRICT)

    def test_threaded_calls(self):
        def encode(_):
            return self.spec.encode_form(0, [1, -1]).data
        with ThreadPoolExecutor(max_workers=4) as pool:
            self.assertEqual(list(pool.map(encode, range(64))), [bytes.fromhex("01 ff 00 00")] * 64)

    def test_real_architecture(self):
        with ma.load(ROOT / "ArchDB/riscv64.dsymspec.yaml") as spec:
            self.assertEqual(spec.encode("addi", {"rd": 10, "rs1": 10, "imm": -1}).data,
                             bytes.fromhex("13 05 f5 ff"))

    def test_all_catalogue_specs(self):
        paths = sorted((ROOT / "ArchDB").glob("*.dsymspec.yaml"))
        self.assertEqual(len(paths), 28)
        for path in paths:
            with self.subTest(arch=path.name), ma.load(path) as spec:
                self.assertGreater(spec.target_count, 0)
                self.assertTrue(spec.target(0).arch)


if __name__ == "__main__":
    unittest.main()
