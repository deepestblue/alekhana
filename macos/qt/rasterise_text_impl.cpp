#include <filesystem>

#ifdef DEBUG
#include <iostream>
#include <format>
#endif

using namespace std;

#include <QtWidgets/QApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QFontDatabase>

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

//
// Unfortunately, void is irregular in C++,
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0146r1.html
// so we’re forced to use some hackery to regularise it.
// https://stackoverflow.com/questions/47996550/handling-void-assignment-in-c-generic-programming
//
struct Or_void {};
template<typename T>
auto &&operator ,(
    T &&x,
    Or_void
) {
    return forward<T>(x);
}

struct App_font {
    App_font(
        const path &typeface_file_path
    ) :
        app_font_id(
            QFontDatabase::addApplicationFont(
                QString::fromStdString(
                    absolute(typeface_file_path)
                )
            )
        )
    {
        throw_if_failed(app_font_id >= 0);
    }
    ~App_font() {
        static_cast<void>(
            QFontDatabase::removeApplicationFont(
                app_font_id
            )
        );
    }

    auto
    get_typeface_name() const {
        const auto &list = QFontDatabase::applicationFontFamilies(
            app_font_id
        );
        throw_if_failed(! list.isEmpty());
        return list.at(0);
    }

private:
    int app_font_id;
};

class Renderer::impl {
public:
    impl(
        const path &typeface_file_path,
        int argc,
        char *argv[]
    ) :
    app(
        argc,
        argv
    ),
    app_font(
        typeface_file_path
    ),
    qfont(
        app_font.get_typeface_name(),
        static_cast<int>(typeface_size_pt)
    ),
    metrics(
        qfont
    ),
    dummy(
        1, // Has to be non-zero sized for QPainter::begin(…) to succeed
        1,
        QImage::Format_RGB32
    ) {
#ifdef DEBUG
        const auto typeface_name = app_font.get_typeface_name().toStdString();
        cout << format(
            "Typeface: {}, ascent: {}, descent: {}, leading: {}.\n",
            typeface_name,
            metrics.ascent(),
            metrics.descent(),
            metrics.leading()
        );
#endif

        qfont.setStyleStrategy(
            static_cast<QFont::StyleStrategy>(
                QFont::NoAntialias | QFont::NoFontMerging
            )
        );
    }

    auto
    operator()(
        const string &text,
        const string &output_filename
    ) {
        const auto qtext = QString::fromStdString(
            text
        );

        const auto bounding_rect = get_bounding_rect(
            qtext
        );
        const auto image = render_text(
            qtext,
            bounding_rect
        );

        throw_if_failed(
            image.save(
                QString::fromStdString(
                    output_filename
                )
            )
        );
    }

private:
    QRectF
    get_bounding_rect(
        const QString &qtext
    ) {
        return paint_on(
            dummy,
            [&]() {
                const auto rect = metrics.tightBoundingRect(qtext);
#ifdef DEBUG
                cout << format(
                    "For string {}, bounding box: X: {}, Width: {}, Y: {}, Height: {}.\n",
                    qtext.toStdString(),
                    rect.x(),
                    rect.width(),
                    rect.y(),
                    rect.height()
                );
#endif
                return rect;
            }
        );
    }

    QImage
    render_text(
        const QString &qtext,
        const QRectF &bounding_rect
    ) {
        auto image = QImage{
            static_cast<int>(ceil(bounding_rect.width())),
            static_cast<int>(ceil(bounding_rect.height() + 35)), // We seem to need a bit more height on Qt. Probably a typeface bug?
            QImage::Format_RGB32
        };
        image.fill(
            Qt::white
        );
        paint_on(
            image,
            [&]() {
                painter.drawText(
                    - static_cast<int>(ceil(bounding_rect.x())),
                    - static_cast<int>(ceil(bounding_rect.y())) + 5, // What's going on here?
                    qtext
                );
            }
        );
        return image;
    }

    template <typename F>
    auto
    paint_on(
        QImage &image,
        F fn
    ) -> decltype(fn()) {
        throw_if_failed(
            painter.begin(
                &image
            )
        );
        painter.setFont(
            qfont
        );
        const auto result = (
            fn(),
            Or_void{}
        );
        throw_if_failed(
            painter.end()
        );
        return static_cast<decltype(fn())>(
            result
        );
    }

    QApplication app;

    App_font app_font;
    QFont qfont;
    QFontMetrics metrics;

    QPainter painter;
    QImage dummy;
};

Renderer::Renderer(
    const path &typeface_file_path,
    int argc,
    char *argv[]
) : p_impl{
    std::make_unique<impl>(
        typeface_file_path,
        argc,
        argv
    )
} {}

void
Renderer::operator()(
    const string &text,
    const string &output_filename
) const {
    (*p_impl)(
        text,
        output_filename
    );
}

Renderer::~Renderer() = default;
