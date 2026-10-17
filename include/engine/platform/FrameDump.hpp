#pragma once

#include <iosfwd>
#include <string>

#include "engine/core/Status.hpp"
#include "engine/renderer/Framebuffer.hpp"

namespace ge {

// Binary PPM (P6). Alpha is dropped.
Status write_ppm(const Framebuffer& frame, const std::string& path);

// Luminance ramp rendering for terminals; characters are about twice as tall as wide.
void write_ascii(const Framebuffer& frame, std::ostream& out, int columns);

}  // namespace ge
