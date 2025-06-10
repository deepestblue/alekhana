#pragma warning(disable: 4820)
#pragma warning(disable: 4464)

#pragma warning(disable: 5045)

#include <sstream>
#include <iomanip>
#include <limits>
#include <filesystem>

//#define DEBUG

#ifdef DEBUG
#include <iostream>
#include <format>
#endif

#pragma warning(default: 5045)

#define NOMINMAX

#include <comdef.h>
#include <wrl/client.h>
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <wincodec.h>

using namespace std;

#include "../rasterise_text.hpp"

using Microsoft::WRL::ComPtr;
using D2D1::ColorF;
using D2D1::RectF;
using D2D1::PixelFormat;
using D2D1::Matrix3x2F;

auto
throw_if_failed(int win32_return_code) {
    const auto win32_error_msg = [] {
        const auto last_error = GetLastError();
        const auto buffer = unique_ptr<char, decltype(&LocalFree)>(
            nullptr,
            LocalFree
        );
        const auto len = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            last_error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buffer.get(),
            0,
            nullptr
        );

        return string{buffer.get(), len};
    };

    throw_if_failed(
        win32_return_code > 0,
        win32_error_msg
    );
}

auto
utf8_to_utf16(const string &in) {
    const auto buf_size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        in.data(),
        static_cast<int>(in.length()),
        nullptr,
        0
    );
    throw_if_failed(buf_size);

    auto out = wstring(static_cast<size_t>(buf_size), 0);
    throw_if_failed(
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            in.data(),
            static_cast<int>(in.length()),
            out.data(),
            buf_size
        )
    );
    return out;
}

auto
throw_if_failed(HRESULT hr) {
    struct com_error_msg {
        com_error_msg(HRESULT hr)
        : hresult(hr) {}

        string
        operator()() const {
            _com_error com_error(hresult);
            auto s = stringstream{};
            s << "Failure with HRESULT of 0x"s;
            s << setfill('0') << setw(sizeof(HRESULT) * 2) // 2 hex digits per char
                << hex << static_cast<unsigned int>(hresult);
            s << setw(0) << " ("s << com_error.ErrorMessage() << " )\n"s;

            return s.str();
        }
    private:
        HRESULT hresult;
    };

    throw_if_failed(
        SUCCEEDED(hr),
        com_error_msg(hr)
    );
}

class COM_initer {
public:
    COM_initer() {
        throw_if_failed(
            CoInitializeEx(
                nullptr,
                COINIT_MULTITHREADED
            )
        );
    }
    ~COM_initer() {
        CoUninitialize();
    }
};

auto
create_font_collection(
    const ComPtr<IDWriteFactory5> &dwrite_factory,
    const path &typeface_file_path
) {
    auto font_set_builder = ComPtr<IDWriteFontSetBuilder1>{};
    throw_if_failed(
        dwrite_factory->CreateFontSetBuilder(&font_set_builder)
    );

    auto font_file = ComPtr<IDWriteFontFile>{};
    throw_if_failed(
        dwrite_factory->CreateFontFileReference(
            absolute(typeface_file_path).c_str(),
            nullptr,
            &font_file
        )
    );

    throw_if_failed(
        font_set_builder->AddFontFile(font_file.Get())
    );

    auto font_set = ComPtr<IDWriteFontSet>{};
    throw_if_failed(
        font_set_builder->CreateFontSet(&font_set)
    );

    auto font_collection = ComPtr<IDWriteFontCollection1>{};
    throw_if_failed(
        dwrite_factory->CreateFontCollectionFromFontSet(
            font_set.Get(),
            &font_collection
        )
    );

    auto font_family = ComPtr<IDWriteFontFamily>{};
    throw_if_failed(
        font_collection->GetFontFamily(
            0,
            &font_family
        )
    );

    auto family_names = ComPtr<IDWriteLocalizedStrings>{};
    throw_if_failed(
        font_family->GetFamilyNames(&family_names)
    );

    const auto count = family_names->GetCount();
    throw_if_failed(
        count > 0,
        [] { return "Typeface has no names."s; }
    );

    auto buf_size = unsigned int{};
    throw_if_failed(
        family_names->GetStringLength(
            0,
            &buf_size
        )
    );
    ++buf_size;

    auto typeface_name = wstring(buf_size, 0);
    throw_if_failed(
        family_names->GetString(
            0,
            typeface_name.data(),
            buf_size
        )
    );

    return pair<ComPtr<IDWriteFontCollection>, wstring>{
        font_collection,
        typeface_name
    };
}

auto
create_dwrite_text_layout(
    const ComPtr<IDWriteFactory5> &dwrite_factory,
    const ComPtr<IDWriteTextFormat> &text_format,
    const string &text
) {
    const auto utf16_text = utf8_to_utf16(text);

    auto dwrite_text_layout = ComPtr<IDWriteTextLayout>{};
    throw_if_failed(
        dwrite_factory->CreateTextLayout(
            utf16_text.data(),
            static_cast<uint32_t>(utf16_text.length()),
            text_format.Get(),
            numeric_limits<float>::max(),
            numeric_limits<float>::max(),
            &dwrite_text_layout
        )
    );

    return dwrite_text_layout;
}

auto
create_render_target(
    const ComPtr<ID2D1Factory1> &d2d_factory,
    const ComPtr<IWICBitmap> &wic_bitmap
) {
    auto render_target = ComPtr<ID2D1RenderTarget>{};

    const auto pixel_format = PixelFormat(
        DXGI_FORMAT_UNKNOWN,
        D2D1_ALPHA_MODE_IGNORE
    );
    const auto render_props = D2D1_RENDER_TARGET_PROPERTIES{
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        pixel_format,
        0,
        0,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT,
    };
    throw_if_failed(
        d2d_factory->CreateWicBitmapRenderTarget(
            wic_bitmap.Get(),
            &render_props,
            &render_target
        )
    );

    render_target->SetTextAntialiasMode(
        D2D1_TEXT_ANTIALIAS_MODE::D2D1_TEXT_ANTIALIAS_MODE_ALIASED
    );
    return render_target;
}

auto
encode_wicbitmap_onto_wicstream(
    const ComPtr<IWICImagingFactory2> &wic_factory,
    IWICStream *stream,
    const ComPtr<IWICBitmap> &wic_bitmap
) {
    auto wic_bitmap_encoder = static_cast<IWICBitmapEncoder *>(nullptr);
    throw_if_failed(
        wic_factory->CreateEncoder(
            GUID_ContainerFormatPng,
            nullptr,
            &wic_bitmap_encoder
        )
    );

    throw_if_failed(
        wic_bitmap_encoder->Initialize(
            stream,
            WICBitmapEncoderNoCache
        )
    );

    {
        auto wic_frame_encode = static_cast<IWICBitmapFrameEncode *>(nullptr);
        throw_if_failed(
            wic_bitmap_encoder->CreateNewFrame(
                &wic_frame_encode,
                nullptr
            )
        );
        throw_if_failed(
            wic_frame_encode->Initialize(nullptr)
        );
        throw_if_failed(
            wic_frame_encode->WriteSource(
                wic_bitmap.Get(),
                nullptr
            )
        );
        throw_if_failed(
            wic_frame_encode->Commit()
        );
    }

    throw_if_failed(
        wic_bitmap_encoder->Commit()
    );
}

class Renderer::impl {
public:
    impl(
        const path &typeface_file_path
    )
    {
        throw_if_failed(
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&wic_factory)
            )
        );

        const auto options = D2D1_FACTORY_OPTIONS{};
        throw_if_failed(
            D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED,
                __uuidof(d2d_factory),
                &options,
                &d2d_factory
            )
        );

        {
            throw_if_failed(
                DWriteCreateFactory(
                    DWRITE_FACTORY_TYPE_SHARED,
                    __uuidof(dwrite_factory),
                    &dwrite_factory
                )
            );

            const auto [font_collection, typeface_name] = create_font_collection(
                dwrite_factory,
                typeface_file_path
            );

            throw_if_failed(
                dwrite_factory->CreateTextFormat(
                    typeface_name.c_str(),
                    font_collection.Get(),
                    DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL,
                    DWRITE_FONT_STRETCH_NORMAL,
                    typeface_size_pt,
                    L"",
                    &text_format
                )
            );
        }
    }

    auto
    operator()(
        const string &text,
        const string &output_filename
    ) const {
        const auto dwrite_text_layout = create_dwrite_text_layout(
            dwrite_factory,
            text_format,
            text
        );

        auto metrics = DWRITE_TEXT_METRICS{};
        throw_if_failed(
            dwrite_text_layout->GetMetrics(
                &metrics
            )
        );

        auto overhang_metrics = DWRITE_OVERHANG_METRICS{};
        throw_if_failed(
            dwrite_text_layout->GetOverhangMetrics(
                &overhang_metrics
            )
        );

#ifdef DEBUG
        cout << format(
            "Metrics for {} are as follows:\nWidth: {}\nHeight: {}\nOverhang left: {}\nOverhang top: {}\n",
            text,
            metrics.width,
            metrics.height,
            overhang_metrics.left,
            overhang_metrics.top
        );
#endif

        auto wic_bitmap = ComPtr<IWICBitmap>{};
        throw_if_failed(
            wic_factory->CreateBitmap(
                static_cast<unsigned int>(metrics.width - overhang_metrics.left),
                static_cast<unsigned int>(metrics.height - overhang_metrics.top),
                GUID_WICPixelFormat32bppBGR,
                WICBitmapCacheOnDemand,
                &wic_bitmap
            )
        );

        auto render_target = create_render_target(
            d2d_factory,
            wic_bitmap
        );

        render_target->BeginDraw();
        render_target->Clear(
            ColorF(ColorF::White)
        );

        auto black_brush = ComPtr<ID2D1SolidColorBrush>{};
        throw_if_failed(
            render_target->CreateSolidColorBrush(
                ColorF(ColorF::Black),
                &black_brush
            )
        );
        render_target->DrawTextLayout(
            D2D1_POINT_2F{
                overhang_metrics.left,
                overhang_metrics.top
            },
            dwrite_text_layout.Get(),
            black_brush.Get(),
            D2D1_DRAW_TEXT_OPTIONS_NONE
        );

        throw_if_failed(
            render_target->EndDraw()
        );

        auto stream = static_cast<IWICStream *>(nullptr);
        throw_if_failed(
            wic_factory->CreateStream(&stream)
        );
        throw_if_failed(
            stream->InitializeFromFilename(
                utf8_to_utf16(output_filename).c_str(),
                GENERIC_WRITE
            )
        );

        encode_wicbitmap_onto_wicstream(
            wic_factory,
            stream,
            wic_bitmap
        );
    }

private:
    COM_initer com_initer;

    ComPtr<IWICImagingFactory2> wic_factory;
    ComPtr<IDWriteTextFormat> text_format;
    ComPtr<IDWriteFactory5> dwrite_factory;
    ComPtr<ID2D1Factory1> d2d_factory;
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
