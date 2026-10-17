#include "engine/platform/FrameDump.hpp"

#include <algorithm>
#include <fstream>
#include <ostream>
#include <vector>

namespace ge {

Status write_ppm(const Framebuffer& frame, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return Status::failure("cannot open '" + path + "' for writing");
    file << "P6\n" << frame.width() << ' ' << frame.height() << "\n255\n";
    std::vector<char> row(static_cast<std::size_t>(frame.width()) * 3);
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            const std::uint32_t c = frame.pixel(x, y);
            row[static_cast<std::size_t>(x) * 3 + 0] = static_cast<char>(red_of(c));
            row[static_cast<std::size_t>(x) * 3 + 1] = static_cast<char>(green_of(c));
            row[static_cast<std::size_t>(x) * 3 + 2] = static_cast<char>(blue_of(c));
        }
        file.write(row.data(), static_cast<std::streamsize>(row.size()));
    }
    file.flush();
    return file ? Status::success() : Status::failure("write to '" + path + "' failed");
}

void write_ascii(const Framebuffer& frame, std::ostream& out, int columns) {
    if (frame.width() <= 0 || frame.height() <= 0 || columns <= 0) return;
    static constexpr char kRamp[] = " .:-=+*#%@";
    const int cell_w = std::max(1, frame.width() / columns);
    const int cell_h = cell_w * 2;
    for (int y = 0; y + cell_h <= frame.height(); y += cell_h) {
        std::string line;
        for (int x = 0; x + cell_w <= frame.width(); x += cell_w) {
            const std::uint32_t c = frame.pixel(x + cell_w / 2, y + cell_h / 2);
            const unsigned luma = (red_of(c) * 30u + green_of(c) * 59u + blue_of(c) * 11u) / 100u;
            line.push_back(kRamp[std::min(9u, luma * 10u / 256u)]);
        }
        out << line << '\n';
    }
}

}  // namespace ge
