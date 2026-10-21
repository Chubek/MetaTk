import os
from metaarch import *

native = Native()
diagnostics = native.diagnostics_create()
spec = None

try:
    spec = native.spec_load(
        os.fsencode("example.dsymspec"),
        FORMAT_OR_MODE,  # Replace with the value defined by MetaArchABI.h
        FLAGS,           # Replace with supported loading flags
        diagnostics,
    )

    if not spec:
        messages = []
        for index in range(native.diagnostics_count(diagnostics)):
            view = DiagnosticView()
            status = native.diagnostics_get(
                diagnostics, index, C.byref(view)
            )
            # Check status using the success convention in MetaArchABI.h
            if view.message:
                messages.append(view.message.decode("utf-8", errors="replace"))

        raise RuntimeError("\n".join(messages) or "Could not load specification")

    print("Registers:", native.spec_register_count(spec))
    print("Instruction forms:", native.spec_form_count(spec))

finally:
    if spec:
        native.spec_free(spec)
    if diagnostics:
        native.diagnostics_free(diagnostics)

