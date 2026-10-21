package archbfd

import (
	"errors"
	"os"
	"testing"
)

const (
	documents = "../../../ArchBFD"
	catalogue = "../../../ArchBFDCatalogue.yaml"
)

// The object file is produced by the C test; its path comes from the
// environment so that the test does not depend on the build directory.
func objectPath() string { return os.Getenv("METAARCH_ARCHBFD_OBJECT") }

func openCatalogue(t *testing.T) *Catalogue {
	t.Helper()
	db, err := Open(catalogue)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { db.Close() })
	return db
}

func TestCatalogueAndDocuments(t *testing.T) {
	db := openCatalogue(t)
	count, err := db.Count()
	if err != nil {
		t.Fatal(err)
	}
	if count < 8 {
		t.Fatal("catalogue entries", count)
	}
	entry, err := db.Find("ELF64")
	if err != nil {
		t.Fatal(err)
	}
	if entry.Name != "ELF64" || entry.TargetVector != "elf64-x86-64" {
		t.Fatal(entry)
	}
	if _, err := db.Find("no-such-format"); err == nil {
		t.Fatal("unknown format accepted")
	}
	if _, err := db.Entry(count); err == nil {
		t.Fatal("entry bounds not checked")
	}

	doc, err := db.LoadName("ELF64", 0)
	if err != nil {
		t.Fatal(err)
	}
	if doc.Name() != "ELF64" || doc.Format() != "elf" || doc.ByteOrder() != "little" {
		t.Fatal(doc.Name(), doc.Format(), doc.ByteOrder())
	}
	if doc.TargetVector() != "elf64-x86-64" || doc.Architecture() != "x86-64" {
		t.Fatal(doc.TargetVector(), doc.Architecture())
	}
	if doc.Magic() != "7f454c46" {
		t.Fatal(doc.Magic())
	}
	if doc.RecordCount() != 6 || doc.RelocationCount() < 30 {
		t.Fatal(doc.RecordCount(), doc.RelocationCount())
	}
	if doc.ConstantGroupCount() == 0 || doc.SectionCount() == 0 || doc.SourceCount() == 0 {
		t.Fatal("empty catalogue entry")
	}
	source, err := doc.Source(0)
	if err != nil || source.File == "" {
		t.Fatal(source, err)
	}

	record, err := doc.Record(0)
	if err != nil {
		t.Fatal(err)
	}
	if record.Name != "ehdr" || record.Size != 64 || len(record.Fields) != 14 {
		t.Fatal(record)
	}
	if record.Fields[0].Name != "e_ident" || record.Fields[0].Size != 16 {
		t.Fatal(record.Fields[0])
	}
	found, err := doc.FindRecord("shdr")
	if err != nil || found.Size != 64 || len(found.Fields) != 10 {
		t.Fatal(found, err)
	}
	if _, err := doc.FindRecord("nope"); err == nil {
		t.Fatal("unknown record accepted")
	}

	index, err := doc.FindRelocation("R_X86_64_PC32")
	if err != nil || index < 0 {
		t.Fatal(index, err)
	}
	reloc, err := doc.Relocation(int(index))
	if err != nil {
		t.Fatal(err)
	}
	if reloc.Number != 2 || reloc.Size != 4 || reloc.Bitsize != 32 || !reloc.PCRelative {
		t.Fatal(reloc)
	}
	if reloc.Overflow != OverflowSigned {
		t.Fatal(reloc.Overflow)
	}
	byNumber, err := doc.FindRelocationNumber(2)
	if err != nil || byNumber < 0 {
		t.Fatal(byNumber, err)
	}
	if missing, err := doc.FindRelocation("R_X86_64_NOPE"); err != nil || missing != -1 {
		t.Fatal(missing, err)
	}

	group, err := doc.ConstantGroup(0)
	if err != nil || group.Name != "object_types" {
		t.Fatal(group, err)
	}
	shdr, err := doc.Record(2)
	if err != nil || shdr.Name != "shdr" {
		t.Fatal(shdr, err)
	}
	section, err := doc.Section(0)
	if err != nil || section.Name == "" {
		t.Fatal(section, err)
	}
}

func TestDocumentFromDirectory(t *testing.T) {
	db, err := Scan(documents)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	count, err := db.Count()
	if err != nil || count < 8 {
		t.Fatal(count, err)
	}
	entry, err := db.Find("m68k")
	if err != nil {
		t.Fatal(err)
	}
	doc, err := db.Load(entry, 0)
	if err != nil {
		t.Fatal(err)
	}
	if doc.Container() != "ELF32" {
		t.Fatal(doc.Container())
	}
	if doc.Format() != "elf" || doc.ByteOrder() != "big" {
		t.Fatal(doc.Format(), doc.ByteOrder())
	}
}

func TestObjectReader(t *testing.T) {
	object := objectPath()
	if object == "" {
		t.Skip("no test object: set METAARCH_ARCHBFD_OBJECT")
	}
	db := openCatalogue(t)
	doc, err := db.LoadName("ELF64", 0)
	if err != nil {
		t.Fatal(err)
	}
	opened, err := OpenObject(object, doc)
	if err != nil {
		t.Fatal(err)
	}
	defer opened.Close()
	if opened.Kind() != "elf64" || opened.Machine() != 62 || opened.Type() != 1 {
		t.Fatal(opened.Kind(), opened.Machine(), opened.Type())
	}
	if opened.SectionCount() != 6 || opened.SymbolCount() != 3 || opened.RelocationCount() != 1 {
		t.Fatal(opened.SectionCount(), opened.SymbolCount(), opened.RelocationCount())
	}
	section, err := opened.Section(1)
	if err != nil || section.Name != ".text" || section.Size != 16 {
		t.Fatal(section, err)
	}
	symbol, err := opened.Symbol(1)
	if err != nil || symbol.Name != "g" || symbol.ClassValue != 1 {
		t.Fatal(symbol, err)
	}
	reloc, err := opened.Relocation(0)
	if err != nil || reloc.TypeName != "R_X86_64_PC32" || reloc.SymbolName != "g" {
		t.Fatal(reloc, err)
	}
	if reloc.Offset != 8 || reloc.Addend != -4 {
		t.Fatal(reloc)
	}
	if _, err := opened.Section(99); err == nil {
		t.Fatal("section bounds")
	}
	if _, err := opened.Symbol(99); err == nil {
		t.Fatal("symbol bounds")
	}
	if _, err := opened.Relocation(99); err == nil {
		t.Fatal("relocation bounds")
	}
}

func TestFailures(t *testing.T) {
	db := openCatalogue(t)
	if _, err := db.LoadName("no-such-format", 0); err == nil {
		t.Fatal("unknown format accepted")
	}
	var diagnosticError *Error
	_, err := db.LoadName("no-such-format", 0)
	if !errors.As(err, &diagnosticError) {
		t.Fatal(err)
	}
	if _, err := Open("bad\x00path"); err == nil {
		t.Fatal("embedded NUL accepted")
	}
	if err := db.Close(); err != nil {
		t.Fatal(err)
	}
	if err := db.Close(); err != nil {
		t.Fatal(err)
	}
	if _, err := db.Count(); !errors.Is(err, ErrClosed) {
		t.Fatal(err)
	}
	var nilCatalogue *Catalogue
	if err := nilCatalogue.Close(); err != nil {
		t.Fatal(err)
	}
}
