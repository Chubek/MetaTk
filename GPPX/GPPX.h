#ifndef GPPX_ROOT_UMBRELLA_H
#define GPPX_ROOT_UMBRELLA_H
/** \file GPPX.h
 * Single inclusion point for the GPPX public C API and extension helpers.
 * For installed builds, #include <GPPX-all.h>; <GPPX.h> remains the compact
 * versioned engine ABI with minimal dependencies.
 */
#include "include/GPPX.h"
#include "include/GPPX-Directive.h"
#include "include/GPPX-Macro.h"
#include "include/GPPX-Syntax.h"
#include "include/GPPX-Reader.h"
#include "include/GPPX-Sink.h"
#include "include/GPPX-Embed.h"
#include "include/GPPX-Rewriter.h"
#include "include/GPPX-PMatch.h"
#include "include/GPPX-Style.h"
#include "include/GPPX-Layout.h"
#include "include/GPPX-UTF8Input.h"
#include "include/GPPX-MetaSym.h"
#include "include/GPPX-Function.h"
#include "include/GPPX-Profile.h"
#include "include/GPPX-Jackscript.h"
#endif
