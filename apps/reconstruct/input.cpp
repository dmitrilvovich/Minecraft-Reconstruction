#include "input.hpp"
#include "lodepng.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <stdexcept>
#include <string_view>

namespace mcr::app {
namespace {
[[noreturn]] void invalid(const std::string& message) { throw std::invalid_argument(message); }
std::vector<unsigned char> read_file(const std::filesystem::path& path, std::size_t limit) {
    std::ifstream file(path, std::ios::binary);
    if (!file) invalid("cannot open input: " + path.string());
    std::vector<unsigned char> data;
    for (char c; file.get(c);) {
        if (data.size() == limit) invalid("input exceeds size limit: " + path.string());
        data.push_back(static_cast<unsigned char>(c));
    }
    if (!file.eof()) invalid("input read failed: " + path.string());
    return data;
}

// PicoJSON's documented streaming interface, specialized to reject duplicate
// keys and excessive nesting. The vendored dependency itself is unmodified.
class StrictContext : public picojson::default_parse_context {
    unsigned depth_;
public:
    StrictContext(Json* out, unsigned depth = 0) : default_parse_context(out), depth_(depth) {}
    bool parse_array_start() { return depth_ < 16 && default_parse_context::parse_array_start(); }
    bool parse_object_start() { return depth_ < 16 && default_parse_context::parse_object_start(); }
    template<class Iter> bool parse_array_item(picojson::input<Iter>& in, std::size_t) {
        auto& a = out_->get<Array>(); a.emplace_back();
        StrictContext child(&a.back(), depth_ + 1);
        return picojson::_parse(child, in);
    }
    template<class Iter> bool parse_object_item(picojson::input<Iter>& in, const std::string& key) {
        auto& o = out_->get<Object>();
        if (o.contains(key)) return false;
        StrictContext child(&o[key], depth_ + 1);
        return picojson::_parse(child, in);
    }
};

Json parse_json(const std::vector<unsigned char>& bytes) {
    const std::string text(bytes.begin(), bytes.end());
    // The schema uses ASCII identifiers and rational strings only. Strict
    // numeric lexical validation closes the parser's permissive strtod path.
    bool quoted = false, escaped = false;
    // Accept integer tokens only: a decimal close to an integer must not be
    // rounded by the JSON parser before exactness checks. Use "n/d" for ratios.
    const std::regex number(R"(-?(0|[1-9][0-9]*))");
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c > 127 || c == 0) invalid("metadata must be ASCII JSON");
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '-' || (c >= '0' && c <= '9')) {
            const auto start = i;
            while (i < text.size() && std::string_view(" \t\n\r,]}:").find(text[i]) == std::string_view::npos) ++i;
            if (!std::regex_match(text.substr(start, i-start), number)) invalid("metadata numbers must use integer literals or rational strings; decimals/exponents are unsupported");
            --i;
        }
    }
    Json result; StrictContext context(&result); std::string error;
    const auto end = picojson::_parse(context, text.begin(), text.end(), &error);
    if (!error.empty()) invalid("invalid metadata JSON (syntax, duplicate key or nesting): " + error);
    if (!std::all_of(end, text.end(), [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }))
        invalid("trailing data after metadata JSON");
    return result;
}

const Object& object(const Json& value, std::initializer_list<const char*> names, const char* where) {
    if (!value.is<Object>()) invalid(std::string(where) + " must be an object");
    const auto& result = value.get<Object>();
    std::set<std::string> expected;
    for (const auto* name : names) expected.emplace(name);
    if (result.size() != expected.size()) invalid(std::string(where) + " has missing or unknown fields");
    for (const auto& [key, unused] : result) {
        (void)unused;
        if (!expected.contains(key)) invalid(std::string(where) + " has unknown field: " + key);
    }
    return result;
}
std::string string(const Json& value) {
    if (!value.is<std::string>()) invalid("expected a JSON string");
    return value.get<std::string>();
}
std::int64_t integer(const Json& value) {
    if (!value.is<double>()) invalid("expected a JSON integer");
    const double x = value.get<double>();
    if (!std::isfinite(x) || std::trunc(x) != x || std::abs(x) > 1000000)
        invalid("integer must be exact and within [-1000000,1000000]");
    return static_cast<std::int64_t>(x);
}
std::int64_t rational_integer(std::string_view text) {
    std::int64_t n = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data()+text.size(), n);
    if (error != std::errc{} || end != text.data()+text.size() || n < -1000000 || n > 1000000)
        invalid("rational numerator/denominator must be an integer within +/-1000000");
    return n;
}
Rational rational(const Json& value) {
    if (value.is<double>()) return Rational(integer(value));
    const auto s = string(value); const auto slash = s.find('/');
    if (slash == std::string::npos) return Rational(rational_integer(s));
    const auto denominator = rational_integer(std::string_view(s).substr(slash+1));
    if (denominator <= 0) invalid("rational denominator must be positive");
    return {rational_integer(std::string_view(s).substr(0,slash)), denominator};
}
Vec3 vector(const Json& value) {
    if (!value.is<Array>() || value.get<Array>().size() != 3) invalid("expected a length-three vector");
    const auto& a = value.get<Array>();
    return {rational(a[0]), rational(a[1]), rational(a[2])};
}
std::uint32_t u32(const unsigned char* b) {
    return (static_cast<std::uint32_t>(b[0])<<24) | (static_cast<std::uint32_t>(b[1])<<16) |
           (static_cast<std::uint32_t>(b[2])<<8) | b[3];
}
void check_chunks(const std::vector<unsigned char>& data) {
    constexpr std::array<unsigned char,8> signature{137,80,78,71,13,10,26,10};
    if (data.size() < 8 || !std::equal(signature.begin(), signature.end(), data.begin())) invalid("not a PNG file");
    bool header = false, image = false, ended_idat = false;
    for (std::size_t pos = 8; pos < data.size();) {
        if (data.size()-pos < 12) invalid("truncated PNG chunk");
        const auto n = static_cast<std::size_t>(u32(data.data()+pos));
        if (n > data.size()-pos-12) invalid("truncated PNG chunk payload");
        const std::string type(reinterpret_cast<const char*>(data.data()+pos+4), 4);
        if (!std::all_of(type.begin(),type.end(),[](char c) {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            }) || type[2] < 'A' || type[2] > 'Z') invalid("invalid PNG chunk type");
        if (lodepng_crc32(data.data()+pos+4, n+4) != u32(data.data()+pos+8+n)) invalid("PNG CRC mismatch");
        if (!header && (type != "IHDR" || n != 13)) invalid("PNG must start with one IHDR");
        if (type == "IHDR") {
            if (header) invalid("duplicate PNG IHDR");
            header = true;
        }
        if (type == "acTL" || type == "fcTL" || type == "fdAT") invalid("animated PNG is unsupported");
        if (type == "IDAT") {
            if (ended_idat) invalid("nonconsecutive PNG IDAT chunks");
            image = true;
        } else if (image) ended_idat = true;
        pos += n+12;
        if (type == "IEND") {
            if (!image || n != 0 || pos != data.size()) invalid("invalid PNG IEND or trailing data");
            return;
        }
    }
    invalid("missing PNG IEND");
}
} // namespace

Input read_metadata(const std::filesystem::path& path) {
    auto metadata = parse_json(read_file(path, 65536));
    const auto& root = object(metadata, {"schema","grid","palette","pixel_encoding","image","camera"}, "metadata");
    if (string(root.at("schema")) != "mcr-a2-camera-v1") invalid("unsupported metadata schema");
    if (string(root.at("pixel_encoding")) != "mcr-a2-first-hit-rgb-v1") invalid("unsupported pixel encoding");
    const auto& g = object(root.at("grid"), {"shape","origin","cell_size"}, "grid");
    if (vector(g.at("shape")) != Vec3{2,2,2} || vector(g.at("origin")) != Vec3{} || rational(g.at("cell_size")) != 1)
        invalid("supported grid is exactly shape [2,2,2], origin [0,0,0], unit cells");
    const auto palette_name = string(root.at("palette"));
    if (palette_name != "split_material" && palette_name != "same_material") invalid("unsupported A2 palette");
    const auto palette = palette_name == "split_material" ? A2Palette::split_material : A2Palette::same_material;
    const auto& im = object(root.at("image"), {"width","height","origin"}, "image");
    const auto width = integer(im.at("width")), height = integer(im.at("height"));
    if (width < 1 || height < 1 || width > 64 || height > 64) invalid("image dimensions must be within 1..64");
    if (string(im.at("origin")) != "top_left") invalid("image origin must be top_left");
    const auto& c = object(root.at("camera"), {"center","forward","right","up","fx","fy","cx","cy"}, "camera");
    PinholeCamera camera{"input",vector(c.at("center")),vector(c.at("forward")),vector(c.at("right")),vector(c.at("up")),
        static_cast<int>(width),static_cast<int>(height),rational(c.at("fx")),rational(c.at("fy")),
        rational(c.at("cx")),Rational(height)-rational(c.at("cy"))};
    camera.validate();
    return {Grid{},std::move(camera),palette,std::move(metadata)};
}

Pixels read_png(const std::filesystem::path& path, const Input& input) {
    const auto data = read_file(path, 1024*1024);
    check_chunks(data);
    lodepng::State state;
    unsigned w = 0, h = 0;
    auto error = lodepng_inspect(&w, &h, &state, data.data(), data.size());
    if (error) invalid(std::string("PNG header: ") + lodepng_error_text(error));
    if (w != static_cast<unsigned>(input.camera.width) || h != static_cast<unsigned>(input.camera.height))
        invalid("PNG dimensions do not match camera metadata");
    if (state.info_png.color.bitdepth != 8 || (state.info_png.color.colortype != LCT_RGB && state.info_png.color.colortype != LCT_RGBA) ||
        state.info_png.interlace_method != 0)
        invalid("supported PNG format is noninterlaced 8-bit RGB or opaque RGBA");
    state.decoder.zlibsettings.max_output_size = static_cast<std::size_t>(h)*(1+4*w);
    state.decoder.read_text_chunks = 0;
    state.decoder.max_icc_size = 65536;
    std::vector<unsigned char> rgba;
    error = lodepng::decode(rgba, w, h, state, data);
    if (error) invalid(std::string("PNG decoding: ") + lodepng_error_text(error));
    if (rgba.size() != static_cast<std::size_t>(w)*h*4) invalid("decoded PNG size mismatch");
    Pixels pixels{w,h,{}}; pixels.labels.reserve(static_cast<std::size_t>(w)*h);
    constexpr std::array<std::array<unsigned char,3>,3> colors{{{135,206,235},{128,128,128},{160,112,64}}};
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        if (rgba[i+3] != 255) invalid("transparent PNG pixels are unsupported");
        const std::array<unsigned char,3> rgb{rgba[i],rgba[i+1],rgba[i+2]};
        const auto found = std::find(colors.begin(), colors.end(), rgb);
        if (found == colors.end()) invalid("unsupported RGB color at PNG pixel (" +
            std::to_string((i/4)%w) + "," + std::to_string((i/4)/w) + ")");
        pixels.labels.push_back(static_cast<PixelLabel>(std::distance(colors.begin(), found)));
    }
    return pixels;
}

Ray pixel_ray(const Input& input, unsigned column, unsigned png_row) {
    return input.camera.pixel(static_cast<int>(column), input.camera.height-1-static_cast<int>(png_row));
}
} // namespace mcr::app
