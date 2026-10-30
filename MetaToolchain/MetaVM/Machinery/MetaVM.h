#ifndef METAVM_H
#define METAVM_H
#define METAVM_VERSION_MAJOR 0
#define METAVM_VERSION_MINOR 1
#define METAVM_VERSION_PATCH 0
#include "System.h"
#include "Value.h"
#include "AbstractMachine.h"
#include "Module.h"
#include "ExecutionEngine.h"
#include "Exec-Bytecode.h"
#include "Exec-ThreadedCode.h"
#include "CBridge.h"
#include "Object.h"
#include "MetaObject.h"
#include "Memory.h"
#include "GC.h"
#include "MetaBuffer.h"
#include "Reader.h"
#include "BinarySerialization.h"
#include "Compiler.h"
#include "TerseSyntax.h"
#include "Optimization.h"
#include "AbstractMachine-Rewrite.h"
#include "MetaIL-ISel.h"
#include "MetaIL-Rewrite.h"
#include "Cache.h"
#include "OS.h"
#include "Paths.h"
#include "Loader.h"
#endif
