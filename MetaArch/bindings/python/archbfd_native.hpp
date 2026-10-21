// The ArchBFD part of the nanobind extension registers its classes in the
// same module as the architecture API.
#ifndef METAARCH_PYTHON_ARCHBFD_NATIVE_HPP
#define METAARCH_PYTHON_ARCHBFD_NATIVE_HPP

#include <nanobind/nanobind.h>

void archbfd_register(nanobind::module_ &m);

#endif
