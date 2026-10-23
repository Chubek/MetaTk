Build and run from the MetaNum directory:
  g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I. tests/test_main.cpp tests/test_other.cpp -o metanum_tests && ./metanum_tests
Regenerate catalog headers after editing MANIFEST.yaml (needs PyYAML):
  python3 tools/gen_catalogs.py .
