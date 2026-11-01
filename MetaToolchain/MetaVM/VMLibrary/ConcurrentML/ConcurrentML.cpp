#include "ConcurrentML.hpp"
namespace metavm { namespace library {
const Profile& ConcurrentML_profile() {
 static const Profile profile = {"ConcurrentML","runtime","catalog-only","Concurrent ML language and concurrency runtime; guest ISA unspecified",&ConcurrentML_example_module};
 return profile;
}
} }
