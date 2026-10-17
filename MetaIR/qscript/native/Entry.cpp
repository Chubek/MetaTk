#include "qscript/include/QScript-Native.h"

extern "C" const qs_native_unit qs_native_unit_data;
extern "C" int64_t qs_native_entry(void*);
int main() { return qs_native_launch(&qs_native_unit_data, qs_native_entry); }
