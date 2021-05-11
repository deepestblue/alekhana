#include <stdexcept>
#include <string>
#include <memory>
#include <filesystem>

#ifndef _WIN32
#define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED
#include <boost/stacktrace.hpp>
#include <boost/exception/all.hpp>

using traced = boost::error_info<struct tag_stacktrace, boost::stacktrace::stacktrace>;

template <typename E>
void throw_with_trace(
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
