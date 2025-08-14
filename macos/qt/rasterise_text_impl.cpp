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
#include <QtGui/QTextLayout>
#include <QtGui/QPainterPath>

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

        auto path = QPainterPath{};
        path.addText(
            0,
            0,
            qfont,
            qtext
        );

        const auto bounding_rect = path.boundingRect();

#ifdef DEBUG
        cout << format(
            "For string {}, bounding box: X: {}, Width: {}, Y: {}, Height: {}.\n",
            qtext.toStdString(),
            bounding_rect.x(),
            bounding_rect.width(),
            bounding_rect.y(),
            bounding_rect.height()
        );
#endif

        auto image = QImage{
            static_cast<int>(ceil(bounding_rect.width())),
            static_cast<int>(ceil(bounding_rect.height())),
            QImage::Format_RGB32
        };
        image.fill(
            Qt::white
        );
        throw_if_failed(
            painter.begin(
                &image
            )
        );

        painter.setFont(
            qfont
        );

        // Position the path to align with the bounding rect
        // The bounding rect's top-left should align with the image's top-left
        painter.translate(
            - static_cast<int>(ceil(bounding_rect.x())),
            - static_cast<int>(ceil(bounding_rect.y()))
        );

        painter.fillPath(
            path,
            Qt::black
        );

        throw_if_failed(
            painter.end()
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
    QApplication app;

    App_font app_font;
    QFont qfont;
    QFontMetrics metrics;

    QPainter painter;
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
