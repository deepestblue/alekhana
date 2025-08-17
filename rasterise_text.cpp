#ifdef _WIN32
#pragma warning(disable: 5045)
#endif

#include <cstdlib>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#pragma warning(default: 5045)
#endif

using namespace std;

#include "rasterise_text.hpp"

int
main(
    int argc,
    char *argv[]
) try {
    throw_if_failed(
        argc == 4,
        [] {
            return "Need 4 arguments."s;
        }
    );
    const auto input_file = string{argv[1]};
    const auto output_dir = string{argv[2]};
    const auto typeface_file_path = path{argv[3]};

    auto renderer = Renderer{
        typeface_file_path,
        argc,
        argv
    };

    auto input_stream = ifstream{input_file};
    throw_if_failed(
        input_stream.is_open(),
        [&] {
            return "Failed to open input file: "s + input_file;
        }
    );

    auto line = string{};
    auto i = size_t{};
    while (
        getline(
            input_stream,
            line
        )
    ) {
        if (line.empty()) {
            continue;
        }
        auto stream = ostringstream{};
        stream << output_dir << "/"s << setfill('0')
            << setw(3) // We don't expect to have any test cases with more than a 1000 lines.
            << i << ".png"s;
        renderer(
            line,
            stream.str()
        );
        ++i;
    }

    return EXIT_SUCCESS;
}
catch (
    const exception &e
) {
    cerr << "Exception thrown: "s << e.what() << '\n';
#ifdef _WIN32
    throw;
#else
    const auto *st = boost::get_error_info<traced>(e);
    if (st) {
        std::cerr << *st << '\n';
    }
    return EXIT_FAILURE;
#endif
}
