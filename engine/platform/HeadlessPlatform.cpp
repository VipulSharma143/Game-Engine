#include "engine/platform/HeadlessPlatform.hpp"

#include <iostream>

#include "engine/core/Logger.hpp"
#include "engine/platform/FrameDump.hpp"

namespace ge {

Status HeadlessPlatform::init(const EngineConfig& config) {
    options_ = config.headless;
    polled_ = presented_ = last_checksum_ = 0;
    return Status::success();
}

void HeadlessPlatform::poll_input(InputState& input) { input = script_.sample(polled_++); }

void HeadlessPlatform::present(const Framebuffer& frame, std::uint64_t) {
    ++presented_;
    last_checksum_ = frame.checksum();
}

void HeadlessPlatform::finalize(const Framebuffer& final_frame) {
    if (!options_.ppm_path.empty()) {
        const Status s = write_ppm(final_frame, options_.ppm_path);
        if (s) GE_LOG_INFO("wrote %s", options_.ppm_path.c_str());
        else GE_LOG_ERROR("%s", s.message.c_str());
    }
    if (options_.ascii_summary) write_ascii(final_frame, std::cout, options_.ascii_columns);
}

}  // namespace ge
