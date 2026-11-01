#include "CPython.hpp"
namespace metavm { namespace library {
const Profile& CPython_profile() {
 static const Profile profile = {"CPython","stack","catalog-only","Python bytecode; version must be pinned",&CPython_example_module};
 return profile;
}
} }
