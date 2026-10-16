%module Chomsky3
%{
#include <chomsky3/chomsky3.h>
#include <stdlib.h>
#include <string.h>
%}
%include "stdint.i"
%include "typemaps.i"
// Match/error structures borrow strings from the engine. Keep their fields
// read-only rather than emitting leaking setters for borrowed const char*.
%immutable;
// Compiled handles are opaque. Call the matching *_free API explicitly;
// contexts must outlive the patterns compiled with them.
%include "chomsky3/error.h"
%include "chomsky3/chomsky3.h"
%inline %{
/* A convenient string-only search that does not expose raw output buffers. */
int chomsky3_binding_search(chomsky3_pattern_t *pattern,const char *text) {
    if (!pattern || !text) return 0;
    return chomsky3_exec(pattern,text,strlen(text),NULL) ? 1 : 0;
}
%}
