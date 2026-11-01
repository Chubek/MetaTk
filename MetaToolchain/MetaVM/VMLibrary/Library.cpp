#include "Library.hpp"
#include "ALEX/ALEX.hpp"
#include "ART/ART.hpp"
#include "AVM2/AVM2.hpp"
#include "B-Code/B-Code.hpp"
#include "BCPL/BCPL.hpp"
#include "BEAM/BEAM.hpp"
#include "C-Machine/C-Machine.hpp"
#include "CAM/CAM.hpp"
#include "CESK/CESK.hpp"
#include "CHIP-8/CHIP-8.hpp"
#include "CLR/CLR.hpp"
#include "CPython/CPython.hpp"
#include "ConcurrentML/ConcurrentML.hpp"
#include "DRMVM/DRMVM.hpp"
#include "DVI/DVI.hpp"
#include "Dalvik/Dalvik.hpp"
#include "Dart/Dart.hpp"
#include "Dis/Dis.hpp"
#include "FAM/FAM.hpp"
#include "FORTH/FORTH.hpp"
#include "FUL/FUL.hpp"
#include "G-Code/G-Code.hpp"
#include "Harrison/Harrison.hpp"
#include "IKL/IKL.hpp"
#include "JSCore/JSCore.hpp"
#include "JVM/JVM.hpp"
#include "Krivine/Krivine.hpp"
#include "LuaVM/LuaVM.hpp"
#include "Lucide/Lucide.hpp"
#include "Monty/Monty.hpp"
#include "MoscowML/MoscowML.hpp"
#include "P-Code/P-Code.hpp"
#include "P4/P4.hpp"
#include "ParrotVM/ParrotVM.hpp"
#include "PostScript/PostScript.hpp"
#include "Prol/Prol.hpp"
#include "SCUMM/SCUMM.hpp"
#include "SECD/SECD.hpp"
#include "SOM/SOM.hpp"
#include "SSAVM/SSAVM.hpp"
#include "Self/Self.hpp"
#include "Smaltalk-80/Smaltalk-80.hpp"
#include "TAMMY/TAMMY.hpp"
#include "V8/V8.hpp"
#include "WAM/WAM.hpp"
#include "WASM/WASM.hpp"
#include "YARV/YARV.hpp"
#include "Z-Machine/Z-Machine.hpp"
#include "Zand/Zand.hpp"
namespace metavm { namespace library {
const std::vector<Profile>& catalog(){
 static const std::vector<Profile> profiles={
ALEX_profile(),
ART_profile(),
AVM2_profile(),
B_Code_profile(),
BCPL_profile(),
BEAM_profile(),
C_Machine_profile(),
CAM_profile(),
CESK_profile(),
CHIP_8_profile(),
CLR_profile(),
CPython_profile(),
ConcurrentML_profile(),
DRMVM_profile(),
DVI_profile(),
Dalvik_profile(),
Dart_profile(),
Dis_profile(),
FAM_profile(),
FORTH_profile(),
FUL_profile(),
G_Code_profile(),
Harrison_profile(),
IKL_profile(),
JSCore_profile(),
JVM_profile(),
Krivine_profile(),
LuaVM_profile(),
Lucide_profile(),
Monty_profile(),
MoscowML_profile(),
P_Code_profile(),
P4_profile(),
ParrotVM_profile(),
PostScript_profile(),
Prol_profile(),
SCUMM_profile(),
SECD_profile(),
SOM_profile(),
SSAVM_profile(),
Self_profile(),
Smaltalk_80_profile(),
TAMMY_profile(),
V8_profile(),
WAM_profile(),
WASM_profile(),
YARV_profile(),
Z_Machine_profile(),
Zand_profile()};return profiles;
}
const Profile* find(std::string_view name){for(const auto&p:catalog())if(name==p.name)return &p;return nullptr;}
}}
