#include "DoxTk.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "test_support.hpp"

using namespace doxtk;

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::filesystem::path root(argv[1]);
    std::filesystem::create_directories(root);
    const auto input = root / "input.bin";
    const auto output = root / "output.txt";
    std::vector<std::byte> binary;
    for (int i = 0; i < 20000; ++i) binary.push_back(static_cast<std::byte>(i % 256));
    CHECK(!write_file(input, binary));
    CHECK(*read_file(input).value == binary);
    CHECK(!write_file(output, {}));
    CHECK(read_file(output).value->empty());
    CHECK(!read_file(root / "does-not-exist"));
    CHECK(!read_file(root));
    CHECK(write_file(root, binary));
    CHECK(write_file(root / "no-such-dir" / "output", binary));

    CHECK(!write_file(output, to_bytes("do not truncate")));
    ConvertOptions options;
    options.source_format = "plaintext";
    options.target_format = "json-dump";
    CHECK(convert_file(input, output, options).code == ErrorCode::parse_error);
    CHECK(*read_file(output).value == to_bytes("do not truncate"));
    options.target_format = "pdf";
    CHECK(convert_file(input, output, options).code == ErrorCode::unsupported_format);
    CHECK(*read_file(output).value == to_bytes("do not truncate"));
    CHECK(!write_file(input, to_bytes("hi\r\n")));
    options.target_format = "plaintext";
    CHECK(!convert_file(input, output, options));
    CHECK(*read_file(output).value == to_bytes("hi\r\n"));
    CHECK(!convert_file(input, input, options));
    CHECK(*read_file(input).value == to_bytes("hi\r\n"));
    CHECK(convert_file(root / "missing", output, options).code == ErrorCode::io_error);
    CHECK(convert_file(input, root / "no-such-dir" / "output", options).code == ErrorCode::io_error);
    CHECK(!write_file(input, {}));
    options.target_format = "xml-dump";
    CHECK(!convert_file(input, output, options));
    CHECK(!read_file(output).value->empty());
    CHECK(!write_file(input, to_bytes("*visible*")));
    options.source_format = "markdown";
    options.target_format = "plaintext";
    auto report = convert_file_with_diagnostics(input, output, options);
    CHECK(!report && report.error.code == ErrorCode::unsupported_conversion);
    options.allow_lossy = true;
    report = convert_file_with_diagnostics(input, output, options);
    CHECK(report && *report.value == output && !report.diagnostics.empty());
    CHECK(*read_file(output).value == to_bytes("visible"));
    report = convert_file_with_diagnostics(input, root / "no-such-dir" / "output", options);
    CHECK(!report && report.error.code == ErrorCode::io_error && !report.diagnostics.empty());
    report = convert_file_with_diagnostics(root / "missing", output, options);
    CHECK(!report && report.error.code == ErrorCode::io_error);
#ifdef __linux__
    // /dev/full succeeds at open; failure appears at buffered write/close.
    CHECK(write_file("/dev/full", to_bytes("buffered write")).code == ErrorCode::io_error);
#endif
}
