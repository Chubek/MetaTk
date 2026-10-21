#include "../xbind_qamrpp_runtime.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ::qamrpp::Context;
using ::qamrpp::Value;
using ::qamrpp::ValuePtr;

ValuePtr borrow(qamrpp_value* value) {
  return value ? ValuePtr(reinterpret_cast<Value*>(value), [](Value*) {})
               : std::make_shared<Value>();
}

qamrpp_value* finish(Context& ctx, const ValuePtr& value) {
  ctx.last_call_returns.clear();
  return reinterpret_cast<qamrpp_value*>(new Value(value ? *value : Value{}));
}

template<class F> qamrpp_value* guard(qamrpp_context* context, F&& fn) {
  auto& ctx = *reinterpret_cast<Context*>(context);
  try {
    return finish(ctx, fn(ctx));
  } catch (const std::exception& error) {
    qamrpp_set_error(context, QAMRPP_ERR_BAD_ARGUMENT, error.what());
    return finish(ctx, nullptr);
  }
}

qamrpp_value* invoke(qamrpp_context* context, qamrpp_value** argv,
                    size_t argc, const char* name) {
  return guard(context, [&](Context& ctx) {
    auto module = xbind::qamrpp::runtime_module(ctx);
    auto function = ctx.table_raw_get(module, std::make_shared<Value>(std::string(name)));
    if (!function || function->type != Value::FUNCTION)
      throw std::runtime_error("unknown XBind constructor: " + std::string(name));
    std::vector<ValuePtr> args;
    for (size_t i = 0; i < argc; ++i) args.push_back(borrow(argv[i]));
    return function->function_value(ctx, args);
  });
}

#define XBIND_BRIDGE(symbol, member) \
  qamrpp_value* symbol(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { \
    return invoke(ctx, argv, argc, member); \
  }
XBIND_BRIDGE(lua_version, "version")
XBIND_BRIDGE(lua_target_count, "target_count")
XBIND_BRIDGE(lua_find_target, "find_target")
XBIND_BRIDGE(lua_target, "target")
XBIND_BRIDGE(lua_typedef, "typedef")
XBIND_BRIDGE(lua_param, "param")
XBIND_BRIDGE(lua_func, "func")
XBIND_BRIDGE(lua_module, "module")
XBIND_BRIDGE(lua_library, "library")
XBIND_BRIDGE(lua_validate_library, "validate_library")
#undef XBIND_BRIDGE

qamrpp_value* lua_require(qamrpp_context* context, qamrpp_value** argv, size_t argc) {
  return guard(context, [&](Context& ctx) {
    auto name = argc ? borrow(argv[0]) : std::make_shared<Value>();
    if (name->type != Value::STRING || (name->string_value != "xbind" && name->string_value != "lxbind"))
      throw std::runtime_error("unknown XBind Lua module");
    return xbind::qamrpp::runtime_module(ctx);
  });
}

const qamrpp_native_binding bindings[] = {
    {"require", lua_require},
    {"xbind_version", lua_version},
    {"xbind_target_count", lua_target_count},
    {"xbind_find_target", lua_find_target},
    {"xbind_library", lua_library},
    {"xbind_target", lua_target},
    {"xbind_typedef", lua_typedef},
    {"xbind_param", lua_param},
    {"xbind_func", lua_func},
    {"xbind_function", lua_func},
    {"xbind_module", lua_module},
    {"xbind_validate_library", lua_validate_library},
};

int on_load(qamrpp_context* context, const qamrpp_host_api*) {
  auto& ctx = *reinterpret_cast<Context*>(context);
  auto module = xbind::qamrpp::runtime_module(ctx);
  ctx.assign_name("xbind", module);
  ctx.assign_name("lxbind", module);
  return 0;
}

const qamrpp_library_descriptor descriptor = {
    QAMRPP_LIBRARY_API_VERSION, "xbind", bindings,
    QAMRPP_ARRAY_COUNT(bindings), on_load, nullptr,
};
} // namespace

extern "C" QAMRPP_EXPORT const qamrpp_library_descriptor* qamrpp_get_library_descriptor(void) {
  return &descriptor;
}
