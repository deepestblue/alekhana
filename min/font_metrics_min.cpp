#include <dwrite.h>
#include <d2d1.h>
#include <wincodec.h>
#include <wrl.h>
#include <string>
#include <iostream>

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

int main() {
    CoInitialize(nullptr);

    // --- Factories ---
    ComPtr<IDWriteFactory> dwriteFactory;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        &dwriteFactory);

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

    // --- Layout ---
    ComPtr<IDWriteTextLayout> textLayout;
    dwriteFactory->CreateTextLayout(
        text.c_str(),
        (UINT32)text.size(),
        textFormat.Get(),
        1000000.0f,
        1000000.0f,
        &textLayout);

    DWRITE_TEXT_METRICS metrics;
    textLayout->GetMetrics(&metrics);

//    std::wcout << L"Text: " << text << std::endl;
    std::wcout << L"Metrics: (l, t, w, h): "
               << L"x=" << metrics.left << L", y=" << metrics.top
               << L", w=" << metrics.width << L", h=" << metrics.height
               << std::endl;

    DWRITE_OVERHANG_METRICS overhangMetrics;
    textLayout->GetOverhangMetrics(&overhangMetrics);

    std::wcout << L"Overhang Metrics: (l, t, r, b): "
               << L"Left " << overhangMetrics.left << L", Top " << overhangMetrics.top
               << L", Right " << overhangMetrics.right << L", Bottom " << overhangMetrics.bottom
               << std::endl;

    // --- Create WIC bitmap sized to bounding box ---
    UINT width  = static_cast<UINT>(ceil(metrics.width + overhangMetrics.left));
    UINT height = static_cast<UINT>(ceil(metrics.height + overhangMetrics.top));

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

    // Shift origin so the text fits within the bitmap (Qt equivalent)
    D2D1_POINT_2F origin = D2D1::Point2F(-metrics.left + overhangMetrics.left, -metrics.top + overhangMetrics.top);
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

    CoUninitialize();
    return 0;
}
