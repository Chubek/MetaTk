#include <DoxTk.hpp>

int main() {
#if defined(DOXTK_ENABLE_CPP_DSL) && DOXTK_ENABLE_CPP_DSL
    doxtk::cppdsl::DocumentDSL d;
    auto built = d.wrap(d.document({d.paragraph({d.text("installed DSL")})})) | d.finish();
    if (!built || !doxtk::builtin_conversions().translate(*built.value, "DoxTkJSON")) return 1;
#endif
    doxtk::ConvertOptions options;
    options.source_format = "plaintext";
    options.target_format = "json-dump";
    return doxtk::convert({}, options) ? 0 : 1;
}
