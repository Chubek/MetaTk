#include "Dart.hpp"
namespace metavm { namespace library {
const Profile& Dart_profile() {
 static const Profile profile = {"Dart","runtime","catalog-only","Dart runtime; guest ABI requires a version",&Dart_example_module};
 return profile;
}
} }
