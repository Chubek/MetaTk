#include "doxtk/DoxTk-CLI.hpp"

int main(int argc, char** argv) {
    return doxtk::run_conversion_cli(argc, argv, "dump2xml", "xml-dump");
}
