#include <MetaAS/MetaAS-DSL.hpp>
metaas::dsl::Program other_program() {
    return metaas::dsl::Program{} | metaas::dsl::encode("ret");
}
