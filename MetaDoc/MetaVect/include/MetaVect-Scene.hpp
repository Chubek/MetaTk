#pragma once
#include "MetaVect-Scene.h"
#include <memory>
#include <stdexcept>
#include <string_view>

namespace metavect {
// Shared immutable ownership makes scene copies and document snapshots safe.
class Scene {
    std::shared_ptr<mv_scene> scene_;
public:
    explicit Scene(std::string_view source) {
        char error[512]{};
        scene_.reset(mv_scene_parse(source.data(),source.size(),error,sizeof(error)),mv_scene_destroy);
        if(!scene_)throw std::runtime_error(error);
    }
    mv_scene_view view() const noexcept {return mv_scene_get_view(scene_.get());}
};
}
