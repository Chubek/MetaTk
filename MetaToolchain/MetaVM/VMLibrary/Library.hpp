#pragma once
#include "MetaVM.h"
#include <string_view>
#include <vector>
namespace metavm { namespace library {
struct Profile {
 const char *name,*architecture,*coverage,*description;
 mvm_status (*example_module)(mvm_module**,mvm_error*);
};
const std::vector<Profile>& catalog();
const Profile* find(std::string_view name);
}}
