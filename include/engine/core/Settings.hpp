#pragma once
// settings.ini: simple "key = value" lines, with a [keys] section for key bindings.
// Lines starting with ';' or '#' are comments. Unknown keys and bad values are reported with
// their line number so typos never pass silently. Command-line options override the file.
//
//   width = 320            height = 200          fov = 66
//   window_scale = 3       textured = true       mouse_sensitivity = 0.0025
//   capture_mouse = true   level = hall
//   [keys]
//   fire = Space,Return

#include <filesystem>
#include <string>
#include <string_view>

#include "engine/core/Config.hpp"
#include "engine/core/Status.hpp"

namespace ge {

Status apply_settings_text(std::string_view text, EngineConfig& config);
// A missing file is an error here; callers that treat the file as optional check existence first.
Status apply_settings_file(const std::filesystem::path& path, EngineConfig& config);

}  // namespace ge
