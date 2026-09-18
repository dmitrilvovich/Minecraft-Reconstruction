#include "mcr/inference/automaton.hpp"
#include <stdexcept>

namespace mcr {
std::optional<RayMode> transition(RayMode mode,PixelLabel emitted,PixelLabel target) {
    if(code(target)>2 || code(emitted)>2) throw std::invalid_argument("invalid pixel label");
    if(mode==RayMode::done) return RayMode::done;
    if(mode!=RayMode::alive) throw std::invalid_argument("invalid automaton mode");
    if(emitted==PixelLabel::background) return RayMode::alive;
    if(emitted==target && target!=PixelLabel::background) return RayMode::done;
    return {};
}
} // namespace mcr
