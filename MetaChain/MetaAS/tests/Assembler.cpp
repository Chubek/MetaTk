#include "MetaAS.hpp"
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void require(bool condition,const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
metaas::Image assemble(const metaas::Assembler& assembler,std::string_view source,const metaas::Options& options={}) {
    auto result=assembler.assemble(source,options,"test.asm");
    require(static_cast<bool>(result),result.diagnostics.empty()?"assembly failed":result.diagnostics.front().message);
    return std::move(*result.value);
}
void bytes(const metaas::Image& image,std::initializer_list<std::uint8_t> expected) {
    auto flat=image.binary();
    require(static_cast<bool>(flat),flat.diagnostics.empty()?"binary failed":flat.diagnostics.front().message);
    require(*flat.value==std::vector<std::uint8_t>(expected),"unexpected bytes");
}
}
int main(int argc,char **argv) {
    try {
        require(argc==3,"expected fixture and MetaArch directory");
        auto loaded=metaas::Assembler::load(argv[1]);
        require(static_cast<bool>(loaded),loaded.diagnostics.empty()?"load failed":loaded.diagnostics.front().message);
        const auto& a=*loaded.value;
        require(a.target_count()==2,"target count");
        require(a.encoders().size()==3,"encoder count");
        bytes(assemble(a,"mov r1, -1\ncopy imm=42, dst=r0\nret"),{0x11,0xff,0x10,42,0xc3});
        auto image=assemble(a,".global start\nstart: mov r1, end-start\n.align 4, 0xcc\nend: ret\n.data\n.word start+1\n.asciz \"a;#\\x42\"\n");
        bytes(image,{0x11,4,0xcc,0xcc,0xc3,1,0,0,0,'a',';','#','B',0});
        require(image.relocations.size()==1,"data relocation retained");
        require(image.symbols.size()==2 && image.symbols[1].global,"global label");
        require(image.json()==image.json(),"deterministic JSON");
        auto unicode=image;
        unicode.abi="\xc3\xa9";
        require(unicode.json().find("\xc3\xa9")!=std::string::npos,"UTF-8 metadata preserved");
        unicode.abi="\xc0\x80";
        bool invalid_utf8=false;
        try { (void)unicode.json(); } catch (const std::invalid_argument&) { invalid_utf8=true; }
        require(invalid_utf8,"invalid UTF-8 metadata rejected");
        auto reloc=assemble(a,".extern external\nbranch external+4\n");
        require(reloc.relocations.size()==1,"ISA relocation count");
        const auto& r=reloc.relocations.front();
        require(r.symbol=="external" && r.offset==1 && r.size==4 && r.bit_offset==5 && r.bit_width==20 && r.big_endian && r.addend==4,"ISA relocation metadata");
        require(!reloc.binary(),"unresolved ISA relocation must fail binary");
        auto negative_addend=assemble(a,".extern ext\nbranch ext-4");
        require(negative_addend.relocations[0].addend==-4,"symbol addend is not an unsigned field value");
        require(!assemble(a,"local: branch local").binary(),"local ISA relocation must require linker");
        require(!assemble(a,".extern ext\n.quad ext").binary(),"external data reference");
        metaas::Options big; big.target=1;
        bytes(assemble(a,".short 0x1234\n.word 0x89abcdef\n",big),{0x12,0x34,0x89,0xab,0xcd,0xef});
        metaas::Options origin; origin.origin=0x1001;
        bytes(assemble(a,".align 4\nlabel: .word label\n",origin),{0,0,0,4,0x10,0,0});
        bytes(assemble(a,".equ N, 2*3+1\n.byte N, (1<<3)|2, 0xff, -128\n.quad -9223372036854775808\n"),{7,10,255,128,0,0,0,0,0,0,0,128});
        bytes(assemble(a,".byte 1\n.data\n.byte 3\n.text\n.byte 2\n.org 4, 7\n"),{1,2,7,7,3});
        const char *bad[]={
            "mov v0, 1", "mov r0, 128", "mov r0, -129", "mov 0, 1", "mov dst=r0, dst=1",
            "mov r0, imm=1", "ret 1", "missing", ".align 3", ".org -1", ".byte 256", ".short -32769",
            ".word undefined", ".global undefined", "x: x: ret", ".equ r0, 1", ".equ X, missing",
            ".byte 1/0", ".byte 1<<64", ".quad 9223372036854775807+1", ".quad -9223372036854775808/-1",
            ".quad -9223372036854775808*-1", ".byte 0b2", ".byte 1,", ".ascii \"\\q\"", ".ascii \"x",
            ".text bad", ".unknown 1", ".extern x\n.word -x", ".extern x, y\n.word x+y",
            "x: .byte 0\n.data\ny: .word x-y", ".byte (1", ".byte 1)", ".equ x, 1\nx: ret"
        };
        for (const auto *source:bad) require(!a.assemble(source),std::string("accepted invalid input: ")+source);
        auto diagnostic=a.assemble("ret\n.byte 999\n",{},"bad.asm");
        require(!diagnostic && diagnostic.diagnostics[0].line==2 && diagnostic.diagnostics[0].file=="bad.asm","positioned diagnostic");
        metaas::Options limits; limits.max_output_bytes=2;
        require(!a.assemble(".zero 3",limits),"output cap");
        limits={}; limits.max_source_bytes=2;
        require(!a.assemble("ret",limits),"source cap");
        limits={}; limits.max_sections=1;
        require(!a.assemble(".data",limits),"section cap");
        limits={}; limits.max_symbols=1;
        require(!a.assemble("x: y: ret",limits),"symbol cap");
        limits={}; limits.max_statements=1;
        require(!a.assemble("x: ret",limits),"statement cap");
        limits={}; limits.max_expression_depth=2;
        require(!a.assemble(".byte (((1)))",limits),"expression cap");
        limits={}; limits.target=99;
        require(!a.assemble("ret",limits),"target validation");
        limits={}; limits.bindings["mov"]={"dst","dst"};
        require(!a.assemble("ret",limits),"binding validation");
        require(!a.assemble(std::string("ret\0ret",7)),"NUL rejected");
        auto ppc=metaas::Assembler::load(std::string(argv[2])+"/ArchDB/ppc32.dsymspec.yaml");
        require(static_cast<bool>(ppc),"load real PPC architecture");
        bytes(assemble(*ppc.value,".insn addi rd=3, ra=3, imm=-1"),{0x38,0x63,0xff,0xff});
        require(!ppc.value->assemble("addi 3,3,-1"),"no guessed positional order");
        std::cout << checks << " checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
