Chapter 2 - Installation and Build
==================================

Configure with CMake and build ``ekippx-cli`` plus the Catch2-based test target.

Install with ``cmake --install <build-dir> --prefix <prefix>``. The install tree exports
``EkippX::ekippx`` under ``<prefix>/lib/cmake/EkippX`` and installs headers under
``<prefix>/include/EkippX`` with flattened compatibility copies for ``DSLtk.hpp`` and
``SerdeTk.hpp`` so downstream ``find_package(EkippX CONFIG REQUIRED)`` consumers build
without extra include-path surgery.
