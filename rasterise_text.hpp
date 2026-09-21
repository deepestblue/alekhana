#include <stdexcept>
#include <string>
#include <memory>
#include <filesystem>
#include <fstream>

#ifndef _WIN32
#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#include <boost/stacktrace.hpp>
#include <boost/exception/all.hpp>

using traced = boost::error_info<struct tag_stacktrace, boost::stacktrace::stacktrace>;

template <typename E>
[[noreturn]] void throw_with_trace(
    const E &e
) {
    throw boost::enable_error_info(e)
        << traced(boost::stacktrace::stacktrace());
}
#endif

using std::string;
using std::unique_ptr;
using std::filesystem::path;

const auto typeface_size_pt = 48u;

// We need to write out empty files to represent a line producing no ink (blank, whitespace‐only, or a zero‐ink glyph such as a notdef fallback), as sadly a 0×0 image isn't representable as an image.
inline void
write_empty_file(
    const path &output_filename
) {
    std::ofstream{output_filename};
}

template <typename E>
auto
throw_if_failed(
    bool exp,
    const E &e
) {
    if (exp)
        return;

#ifdef _WIN32
    throw runtime_error(e());
#else
    throw_with_trace(runtime_error(e()));
#endif
}

class Renderer {
    class impl;
    const unique_ptr<impl> p_impl;
public:
    void
    operator()(
        const string &text,
        const string &output_filename
    ) const;

    Renderer(
        const path &typeface_file_path,
        int argc,
        char *argv[]
    );
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
};
