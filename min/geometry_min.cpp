#include <dwrite_3.h>
#include <d2d1.h>
#include <wincodec.h>
#include <wrl.h>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

int main() {
    CoInitialize(nullptr);

    // --- Factories ---
    ComPtr<IDWriteFactory6> dwriteFactory;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf()));

    ComPtr<ID2D1Factory> d2dFactory;
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2dFactory));

    ComPtr<IWICImagingFactory> wicFactory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&wicFactory));

    // --- Font format ---
    FLOAT fontSizePoints = 72.0f; // nice and big
    FLOAT dpi = 96.0f;
    FLOAT fontEmSize = fontSizePoints * dpi / 72.0f;

    ComPtr<IDWriteTextFormat> textFormat;
    dwriteFactory->CreateTextFormat(
        L"Noto Sans Grantha",
        nullptr,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontEmSize,
        L"sa-IN",
        &textFormat);

    // --- Text ---
    std::wstring text = L"𑌅𑌆𑌇𑌈𑌕𑌖𑌗𑌘𑌙𑌜";

    // --- Get font face and glyph information ---
    ComPtr<IDWriteFontCollection2> fontCollection;
    dwriteFactory->GetSystemFontCollection(false, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &fontCollection);

    UINT32 familyIndex;
    BOOL exists;
    fontCollection->FindFamilyName(L"Noto Sans Grantha", &familyIndex, &exists);

    ComPtr<IDWriteFontFamily2> fontFamily;
    fontCollection->GetFontFamily(familyIndex, &fontFamily);

    ComPtr<IDWriteFont3> font;
    fontFamily->GetFont(0u, &font);

    ComPtr<IDWriteFontFace3> fontFace;
    font->CreateFontFace(&fontFace);

    // --- Get glyph indices ---
    std::vector<UINT16> glyphIndices(text.length());
    std::vector<UINT32> codePoints(text.length());
    for (size_t i = 0; i < text.length(); ++i) {
        codePoints[i] = static_cast<UINT32>(text[i]);
    }

    fontFace->GetGlyphIndices(codePoints.data(), static_cast<UINT32>(codePoints.size()), glyphIndices.data());

    // --- Get glyph outlines and calculate bounding box using ID2D1PathGeometry ---
    ComPtr<ID2D1PathGeometry> pathGeometry;
    d2dFactory->CreatePathGeometry(&pathGeometry);

    ComPtr<ID2D1GeometrySink> geometrySink;
    pathGeometry->Open(&geometrySink);

    HRESULT hr = fontFace->GetGlyphRunOutline(
        fontEmSize,
        glyphIndices.data(),
        nullptr, // glyphAdvances
        nullptr, // glyphOffsets
        static_cast<UINT32>(glyphIndices.size()),
        FALSE,   // isSideways
        FALSE,   // isRightToLeft
        geometrySink.Get()
    );

    geometrySink->Close();

    // Get the bounding box from the path geometry
    D2D1_RECT_F bounds;
    pathGeometry->GetBounds(nullptr, &bounds);

//    std::wcout << L"Text: " << text << std::endl;
    std::wcout << L"Bounding box from glyph outlines: "
               << L"l=" << bounds.left << L", t=" << bounds.top
               << L", r=" << bounds.right << L", b=" << bounds.bottom
               << std::endl;

    // --- Create WIC bitmap sized to bounding box ---
    UINT width = static_cast<UINT>(ceil(bounds.right - bounds.left));
    UINT height = static_cast<UINT>(ceil(bounds.bottom - bounds.top));

    std::wcout << L"Width: " << width << L", Height: " << height << std::endl;

    ComPtr<IWICBitmap> wicBitmap;
    wicFactory->CreateBitmap(
        width, height,
        GUID_WICPixelFormat32bppPBGRA, // premultiplied alpha for D2D
        WICBitmapCacheOnDemand,
        &wicBitmap);

    // --- D2D render target ---
    D2D1_RENDER_TARGET_PROPERTIES renderProps =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
            dpi, dpi);

    ComPtr<ID2D1RenderTarget> renderTarget;
    d2dFactory->CreateWicBitmapRenderTarget(wicBitmap.Get(), &renderProps, &renderTarget);

    renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    // --- Draw ---
    renderTarget->BeginDraw();
    renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    ComPtr<ID2D1SolidColorBrush> blackBrush;
    renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &blackBrush);

    // Create text layout for drawing
    ComPtr<IDWriteTextLayout> textLayout;
    dwriteFactory->CreateTextLayout(
        text.c_str(),
        (UINT32)text.size(),
        textFormat.Get(),
        width,
        height,
        &textLayout);

    // Shift origin so the text fits within the bitmap
    // Adjust for baseline positioning - move text up by the top bound to ensure descenders are visible
    D2D1_POINT_2F origin = D2D1::Point2F(-bounds.left, -bounds.top);
    std::wcout << L"Origin: " << origin.x << L", " << origin.y << std::endl;
    renderTarget->DrawTextLayout(origin, textLayout.Get(), blackBrush.Get(),
                                 D2D1_DRAW_TEXT_OPTIONS_NONE);

    renderTarget->EndDraw();

    // --- Save PNG ---
    ComPtr<IWICStream> stream;
    wicFactory->CreateStream(&stream);
    stream->InitializeFromFilename(L"output.png", GENERIC_WRITE);

    ComPtr<IWICBitmapEncoder> encoder;
    wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    encoder->CreateNewFrame(&frame, &props);
    frame->Initialize(props.Get());
    frame->SetSize(width, height);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    frame->SetPixelFormat(&format);
    frame->WriteSource(wicBitmap.Get(), nullptr);
    frame->Commit();
    encoder->Commit();

    // Clean up
    geometrySink.Reset();
    pathGeometry.Reset();

    CoUninitialize();
    return 0;
}
