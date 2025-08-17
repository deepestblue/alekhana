#include <exception>
#include <array>
#include <filesystem>
#include <cmath>

#ifdef DEBUG
#include <iostream>
#include <format>
#endif

using namespace std;

#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include "../../rasterise_text.hpp"

static auto
throw_if_failed(
    bool exp
) {
    const static auto l = []() {
        return "Assertion failed."s;
    };
    throw_if_failed(
        exp,
        l
    );
}

template<typename T, typename U>
auto
ConstCFReleaser(
    const U &u
) {
    auto ret = unique_ptr<const remove_pointer_t<T>, decltype(&CFRelease)>(
        u,
        [](const void *ref) {
            if (! ref) {
                return;
            }
            CFRelease(ref);
        }
    );
    throw_if_failed(
        ret.get()
    );
    return ret;
}

template<typename T, typename U, typename V>
auto
CFReleaser(
    const U &u,
    const V &v
) {
    auto ret = unique_ptr<remove_pointer_t<T>, decltype(v)>(
        u,
        v
    );
    throw_if_failed(
        ret.get()
    );
    return ret;
}

static auto
CFStringFromString(
    const string &in
) {
    return ConstCFReleaser<CFStringRef>(
            CFStringCreateWithBytes(
                nullptr,
                reinterpret_cast<const unsigned char *>(in.c_str()),
                static_cast<long>(in.length()),
                kCFStringEncodingUTF8,
                false
            )
        );
}

static auto
create_font_from_file(
    const path &typeface_file_path
) {
    const auto &typeface_file_path_string = absolute(typeface_file_path).string();
    const auto typefacePathAsCFString = CFStringFromString(typeface_file_path_string);

    const auto typefaceURL = ConstCFReleaser<CFURLRef>(
        CFURLCreateWithFileSystemPath(
            nullptr,
            typefacePathAsCFString.get(),
            kCFURLPOSIXPathStyle,
            0
        )
    );

    const auto font_descriptors = ConstCFReleaser<CFArrayRef>(
        CTFontManagerCreateFontDescriptorsFromURL(
            typefaceURL.get()
        )
    );

    throw_if_failed(
        CFArrayGetCount(
            font_descriptors.get()
        ) > 0
    );

    const auto font_descriptor = static_cast<CTFontDescriptorRef>(
        CFArrayGetValueAtIndex(
            font_descriptors.get(),
            0
        )
    );

    return ConstCFReleaser<CTFontRef>(
        CTFontCreateWithFontDescriptor(
            font_descriptor,
            typeface_size_pt,
            nullptr
        )
    );
}

class Renderer::impl {
private:
    const unique_ptr<const remove_pointer_t<CTFontRef>, decltype(&CFRelease)> font;

    auto
    create_line(
        const string &text
    ) const {
        auto keys = array<const CFStringRef, 1>{
            kCTFontAttributeName
        };
        auto values = array<const CFTypeRef, keys.size()>{
            font.get()
        };
        const auto attr = ConstCFReleaser<CFDictionaryRef>(
            CFDictionaryCreate(
                nullptr,
                reinterpret_cast<const void **>(&keys),
                reinterpret_cast<const void **>(&values),
                keys.size(),
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks
            )
        );

        const auto textAsCFString = CFStringFromString(text);

        const auto attrString = ConstCFReleaser<CFAttributedStringRef>(
            CFAttributedStringCreate(
                nullptr,
                textAsCFString.get(),
                attr.get()
            )
        );

        return ConstCFReleaser<CTLineRef>(
            CTLineCreateWithAttributedString(
                attrString.get()
            )
        );
    }

public:
    impl(
        const path &typeface_file_path
    ) : font(
        create_font_from_file(typeface_file_path)
    ) {}

    auto
    operator()(
        const string &text,
        const string &output_filename
    ) const {
        const auto line = create_line(
            text
        );

        const auto bounding_box = CTLineGetBoundsWithOptions(
            line.get(),
            kCTLineBoundsUseGlyphPathBounds
        );

#ifdef DEBUG
        cout << format(
            "For string {}, bounding box: X: {}, Width: {}, Y: {}, Height: {}.\n"s,
            text,
            bounding_box.origin.x,
            bounding_box.size.width,
            bounding_box.origin.y,
            bounding_box.size.height
        );
#endif

        const auto context_guard = CFReleaser<CGContextRef>(
            CGBitmapContextCreate(
                nullptr,
                static_cast<size_t>(ceil(bounding_box.size.width)),
                static_cast<size_t>(ceil(bounding_box.size.height)),
                8u,
                0u,
                CGColorSpaceCreateDeviceRGB(),
                kCGImageAlphaPremultipliedLast
            ),
            CGContextRelease
        );
        const auto &context = context_guard.get();

        CGContextSetGrayFillColor(
            context,
            1.,
            1.
        );
        CGContextFillRect(
            context,
            CGRectMake(
                0u,
                0u,
                static_cast<size_t>(ceil(bounding_box.size.width)),
                static_cast<size_t>(ceil(bounding_box.size.height))
            )
        );
        CGContextSetShouldAntialias(
            context,
            false
        );
        CGContextSetTextPosition(
            context,
            -bounding_box.origin.x,
            -bounding_box.origin.y
        );
        CTLineDraw(
            line.get(),
            context
        );

        const auto image = CFReleaser<CGImageRef>(
            CGBitmapContextCreateImage(
                context
            ),
            CGImageRelease
        );

        const auto path = ConstCFReleaser<CFStringRef>(
            CFStringCreateWithCString(
                nullptr,
                output_filename.c_str(),
                kCFStringEncodingUTF8
            )
        );
        const auto destURL = ConstCFReleaser<CFURLRef>(
            CFURLCreateWithFileSystemPath(
                nullptr,
                path.get(),
                kCFURLPOSIXPathStyle,
                0
            )
        );

        const auto imageDestination = CFReleaser<CGImageDestinationRef>(
            CGImageDestinationCreateWithURL(
                destURL.get(),
                static_cast<CFStringRef>(UTTypePNG.identifier),
                1,
                nullptr
            ),
            [](const void *ref) { if (ref) CFRelease(ref); }
        );

        CGImageDestinationAddImage(
            imageDestination.get(),
            image.get(),
            nullptr
        );

        throw_if_failed(
            CGImageDestinationFinalize(
                imageDestination.get()
            )
        );
    }
};

Renderer::Renderer(
    const path &typeface_file_path,
    int ,
    char *[]
) : p_impl{
    std::make_unique<impl>(
        typeface_file_path
    )
} {}

void
Renderer::operator()(
    const string &text,
    const string &output_filename) const {
    (*p_impl)(
        text,
        output_filename
    );
}

Renderer::~Renderer() = default;
