#pragma warning(disable: 4820)
#pragma warning(disable: 4464)

#pragma warning(disable: 5045)

#include <sstream>
#include <iomanip>
#include <limits>
#include <filesystem>
#include <algorithm>

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

auto
throw_if_failed(int win32_return_code) {
    const auto win32_error_msg = [] {
        const auto last_error = GetLastError();
        auto raw_buffer = nullptr;
        const auto len = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            last_error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&raw_buffer),
            0,
            nullptr
        );

        throw_if_failed(
            static_cast<bool>(raw_buffer),
            [] { return "FormatMessageA error"s; }
        );

        auto buffer = unique_ptr<char, decltype(&LocalFree)>(
            raw_buffer,
            LocalFree
        );

        throw_if_failed(
            len > 0,
            [] { return "FormatMessageA error"s; }
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
    auto wic_bitmap_encoder = ComPtr<IWICBitmapEncoder>{};
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
        auto wic_frame_encode = ComPtr<IWICBitmapFrameEncode>{};
        throw_if_failed(
            wic_bitmap_encoder->CreateNewFrame(
                &wic_frame_encode,
                nullptr
            )
        );
        throw_if_failed(
            wic_frame_encode->Initialize(nullptr)
        );

        auto format = WICPixelFormatGUID{GUID_WICPixelFormat32bppBGR};
        throw_if_failed(
            wic_frame_encode->SetPixelFormat(&format)
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

// Sadly, Win32 doesn't seem to offer glyph‐path‐bounds directly, unlike CoreText (CTLineGetBoundsWithOptions(kCTLineBoundsUseGlyphPathBounds)) or Qt (QPainterPath::boundingRect()), so we have to implement our own IDWriteTextRenderer to calculate glyph‐path‐bounds. The reason we need glyph‐path‐bounds is of course that the naive bounding boxvcalculation that DWrite provides relies on DWRITE_TEXT_METRICS/DWRITE_OVERHANG_METRICS. These are sized off the font's design line metrics (ascent/descent/line gap) and not off which pixels actually get inked, and so are wildly wrong often. As above, this implementation unions the exact vector outlines of every glyph run.

class Bounds_renderer : public IDWriteTextRenderer {
public:
    explicit
    Bounds_renderer(
        ID2D1Factory1 *d2d_factory
    ) : d2d_factory(d2d_factory) {}

    virtual ~Bounds_renderer() = default;

    auto
    bounds() const {
        return accumulated_bounds;
    }

    // Bounds_renderer is stack‐allocated and only used for the duration of a single IDWriteTextLayout::Draw call, so refcounting isn't needed.
    IFACEMETHODIMP_(ULONG) AddRef() noexcept override {
        return 1;
    }

    IFACEMETHODIMP_(ULONG) Release() noexcept override {
        return 1;
    }

    IFACEMETHODIMP QueryInterface(
        REFIID riid,
        void **ppv_object
    ) noexcept override {
        if (
            riid != __uuidof(IDWriteTextRenderer) &&
            riid != __uuidof(IDWritePixelSnapping) &&
            riid != __uuidof(IUnknown)
        ) {
            *ppv_object = nullptr;
            return E_NOINTERFACE;
        }

        *ppv_object = this;
        return S_OK;
    }

    IFACEMETHODIMP IsPixelSnappingDisabled(
        void *,
        BOOL *is_disabled
    ) noexcept override {
        *is_disabled = TRUE;
        return S_OK;
    }

    IFACEMETHODIMP GetCurrentTransform(
        void *,
        DWRITE_MATRIX *transform
    ) noexcept override {
        *transform = DWRITE_MATRIX{1, 0, 0, 1, 0, 0};
        return S_OK;
    }

    IFACEMETHODIMP GetPixelsPerDip(
        void *,
        FLOAT *pixels_per_dip
    ) noexcept override {
        *pixels_per_dip = 1.0f;
        return S_OK;
    }

    // IDWriteTextRenderer
    IFACEMETHODIMP DrawGlyphRun(
        void *,
        FLOAT baseline_origin_x,
        FLOAT baseline_origin_y,
        DWRITE_MEASURING_MODE,
        const DWRITE_GLYPH_RUN *glyph_run,
        const DWRITE_GLYPH_RUN_DESCRIPTION *,
        IUnknown *
    ) noexcept override try {
        auto path_geometry = ComPtr<ID2D1PathGeometry>{};
        throw_if_failed(
            d2d_factory->CreatePathGeometry(&path_geometry)
        );

        auto sink = ComPtr<ID2D1GeometrySink>{};
        throw_if_failed(
            path_geometry->Open(&sink)
        );

        throw_if_failed(
            glyph_run->fontFace->GetGlyphRunOutline(
                glyph_run->fontEmSize,
                glyph_run->glyphIndices,
                glyph_run->glyphAdvances,
                glyph_run->glyphOffsets,
                glyph_run->glyphCount,
                glyph_run->isSideways,
                static_cast<BOOL>(glyph_run->bidiLevel % 2),
                sink.Get()
            )
        );

        throw_if_failed(
            sink->Close()
        );

        auto run_bounds = D2D1_RECT_F{};
        throw_if_failed(
            path_geometry->GetBounds(
                nullptr,
                &run_bounds
            )
        );

        // In a different universe, this next check would not be necessary, because any sentinel value we get would be D2D's documented (FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX), which would work cleanly with the min/max calculations. However, we've observed cases where an empty glyph (Sampradaya's U+200C, confirmed with fontTools to have 0 contours and no point data at all) had GetGlyphRunOutline/GetBounds produce instead (inf, inf, FLT_MAX, FLT_MAX), which is not that sentinel and is not safe to blindly union with min/max. So we have to add this explicit check. Boo.
        if (run_bounds.left <= run_bounds.right && run_bounds.top <= run_bounds.bottom) {
            run_bounds.left += baseline_origin_x;
            run_bounds.right += baseline_origin_x;
            run_bounds.top += baseline_origin_y;
            run_bounds.bottom += baseline_origin_y;

            accumulated_bounds.left = min(accumulated_bounds.left, run_bounds.left);
            accumulated_bounds.top = min(accumulated_bounds.top, run_bounds.top);
            accumulated_bounds.right = max(accumulated_bounds.right, run_bounds.right);
            accumulated_bounds.bottom = max(accumulated_bounds.bottom, run_bounds.bottom);
        }

        return S_OK;
    }
    catch ([[maybe_unused]] const exception &e) {
#ifdef DEBUG
        cerr << format(
            "DrawGlyphRun failed: {}\n",
            e.what()
        );
#endif
        return E_FAIL;
    }

    IFACEMETHODIMP DrawUnderline(
        void *,
        FLOAT,
        FLOAT,
        const DWRITE_UNDERLINE *,
        IUnknown *
    ) noexcept override {
        return S_OK;
    }
    IFACEMETHODIMP DrawStrikethrough(
        void *,
        FLOAT,
        FLOAT,
        const DWRITE_STRIKETHROUGH *,
        IUnknown *
    ) noexcept override {
        return S_OK;
    }
    IFACEMETHODIMP DrawInlineObject(
        void *,
        FLOAT,
        FLOAT,
        IDWriteInlineObject *,
        BOOL,
        BOOL,
        IUnknown *
    ) noexcept override {
        return E_NOTIMPL;
    }

private:
    ID2D1Factory1 *d2d_factory;
    D2D1_RECT_F accumulated_bounds{
        numeric_limits<FLOAT>::max(),
        numeric_limits<FLOAT>::max(),
        numeric_limits<FLOAT>::lowest(),
        numeric_limits<FLOAT>::lowest()
    };
};

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

    auto
    operator()(
        const string &text,
        const string &output_filename
    ) const {
        auto dwrite_text_layout = ComPtr<IDWriteTextLayout>{};
        const auto utf16_text = utf8_to_utf16(text);

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

        auto bounds_renderer = Bounds_renderer{d2d_factory.Get()};
        throw_if_failed(
            dwrite_text_layout->Draw(
                nullptr,
                &bounds_renderer,
                0,
                0
            )
        );

        const auto bounds = bounds_renderer.bounds();
        throw_if_failed(
            bounds.left < bounds.right && bounds.top < bounds.bottom,
            [] { return "Text produced no visible glyphs."s; }
        );

#ifdef DEBUG
        cout << format(
            "For string {}, bounding box: X: {}, Width: {}, Y: {}, Height: {}.\n",
            text,
            bounds.left,
            bounds.right - bounds.left,
            bounds.top,
            bounds.bottom - bounds.top
        );
#endif

        const auto width = static_cast<unsigned int>(ceil(bounds.right - bounds.left));
        const auto height = static_cast<unsigned int>(ceil(bounds.bottom - bounds.top));

        auto wic_bitmap = ComPtr<IWICBitmap>{};
        throw_if_failed(
            wic_factory->CreateBitmap(
                width,
                height,
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
                -bounds.left,
                -bounds.top
            },
            dwrite_text_layout.Get(),
            black_brush.Get(),
            D2D1_DRAW_TEXT_OPTIONS_NONE
        );

        throw_if_failed(
            render_target->EndDraw()
        );

        auto stream = ComPtr<IWICStream>{};
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
            stream.Get(),
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
