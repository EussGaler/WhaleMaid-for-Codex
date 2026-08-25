#include "CharacterWidget.hpp"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QMainWindow>
#include <QPalette>
#include <QPixmap>
#include <QSurfaceFormat>
#include <QTimer>

#include <array>
#include <memory>

int main(int argc, char* argv[])
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::NoProfile);
    format.setAlphaBufferSize(8);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WhaleMaid Live2D Window"));
    QApplication::setOrganizationName(QStringLiteral("Codex Desktop Pet"));
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("WhaleMaid Live2D Linux 验证窗口"));
    window.resize(520, 720);
    window.setMinimumSize(260, 360);
    window.setAutoFillBackground(true);
    QPalette palette = window.palette();
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#14213d")));
    window.setPalette(palette);
    window.setCentralWidget(new CharacterWidget(&window));
    window.show();

    const QStringList arguments = app.arguments();
    const qsizetype smokeIndex = arguments.indexOf(QStringLiteral("--smoke-test"));
    if (smokeIndex >= 0 && smokeIndex + 1 < arguments.size())
    {
        const QString screenshotPath = QDir::cleanPath(arguments.at(smokeIndex + 1));
        QTimer::singleShot(2000, &app, [&app, &window, screenshotPath]() {
            app.exit(window.grab().save(screenshotPath, "PNG") ? 0 : 2);
        });
    }

    const qsizetype sequenceIndex = arguments.indexOf(
        QStringLiteral("--smoke-sequence"));
    if (sequenceIndex >= 0 && sequenceIndex + 1 < arguments.size())
    {
        const QString prefix = QDir::cleanPath(arguments.at(sequenceIndex + 1));
        auto savedFrames = std::make_shared<int>(0);
        constexpr std::array frameTimes{800, 1050, 1250, 2700};
        for (std::size_t frame = 0; frame < frameTimes.size(); ++frame)
        {
            QTimer::singleShot(
                frameTimes.at(frame),
                &app,
                [&window, prefix, savedFrames, frame]() {
                    if (window.grab().save(
                            prefix + QStringLiteral("-%1.png").arg(frame + 1),
                            "PNG"))
                    {
                        ++(*savedFrames);
                    }
                });
        }
        QTimer::singleShot(2900, &app, [&app, savedFrames]() {
            app.exit(*savedFrames == 4 ? 0 : 2);
        });
    }

    const qsizetype pointerIndex = arguments.indexOf(
        QStringLiteral("--pointer-smoke"));
    if (pointerIndex >= 0 && pointerIndex + 1 < arguments.size())
    {
        const QString prefix = QDir::cleanPath(arguments.at(pointerIndex + 1));
        const QPoint originalCursorPosition = QCursor::pos();
        auto savedFrames = std::make_shared<int>(0);
        QTimer::singleShot(300, &app, [&window]() {
            QCursor::setPos(window.mapToGlobal(QPoint(20, 120)));
        });
        QTimer::singleShot(900, &app, [&window, prefix, savedFrames]() {
            if (window.grab().save(prefix + QStringLiteral("-left.png"), "PNG"))
            {
                ++(*savedFrames);
            }
        });
        QTimer::singleShot(1100, &app, [&window]() {
            QCursor::setPos(window.mapToGlobal(
                QPoint(window.width() - 20, window.height() - 120)));
        });
        QTimer::singleShot(1700, &app, [&window, prefix, savedFrames]() {
            if (window.grab().save(prefix + QStringLiteral("-right.png"), "PNG"))
            {
                ++(*savedFrames);
            }
        });
        QTimer::singleShot(
            1900,
            &app,
            [&app, savedFrames, originalCursorPosition]() {
                QCursor::setPos(originalCursorPosition);
                app.exit(*savedFrames == 2 ? 0 : 2);
            });
    }

    const qsizetype runForIndex = arguments.indexOf(QStringLiteral("--run-for"));
    if (runForIndex >= 0 && runForIndex + 1 < arguments.size())
    {
        bool validDuration = false;
        const int durationMilliseconds = arguments.at(runForIndex + 1).toInt(
            &validDuration);
        if (validDuration && durationMilliseconds >= 0)
        {
            QTimer::singleShot(durationMilliseconds, &app, &QCoreApplication::quit);
        }
    }

    return app.exec();
}
