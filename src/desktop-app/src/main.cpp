#include "PetWindow.hpp"
#include "CodexStatusBridge.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSurfaceFormat>
#include <QTimer>

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
    QApplication::setApplicationName(QStringLiteral("Whale Maid Pet"));
    QApplication::setOrganizationName(QStringLiteral("Codex Desktop Pet"));
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    const QStringList arguments = app.arguments();
    if (!arguments.contains(QStringLiteral("--hook-autostart")))
    {
        const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
        if (!localAppData.isEmpty())
        {
            QFile::remove(QDir(localAppData).filePath(
                QStringLiteral("WhaleMaid/manual-exit.flag")));
        }
    }
    const qsizetype statusIndex = arguments.indexOf(QStringLiteral("--codex-status"));
    if (statusIndex >= 0 && statusIndex + 1 < arguments.size())
    {
        QString message;
        const qsizetype messageIndex = arguments.indexOf(QStringLiteral("--message"));
        if (messageIndex >= 0 && messageIndex + 1 < arguments.size())
        {
            message = arguments.at(messageIndex + 1);
        }
        return CodexStatusBridge::sendEvent(
                   arguments.at(statusIndex + 1),
                   message,
                   {},
                   {},
                   QStringLiteral("manual"))
            ? 0
            : 3;
    }

    // A normal second launch asks the existing instance to restore itself and exits.
    // Composite UI checks stay isolated from the user's running desktop pet.
    const bool isolatedUiSmoke = arguments.contains(QStringLiteral("--composite-smoke"));
    if (!isolatedUiSmoke && CodexStatusBridge::sendEvent(
            QStringLiteral("activate"), {}, {}, {}, QStringLiteral("activate")))
    {
        return 0;
    }

    PetWindow window;
    const qsizetype windowScaleIndex = arguments.indexOf(QStringLiteral("--window-scale"));
    if (windowScaleIndex >= 0 && windowScaleIndex + 1 < arguments.size())
    {
        bool validScale = false;
        const int requestedScale = arguments.at(windowScaleIndex + 1).toInt(&validScale);
        if (validScale && requestedScale >= 15 && requestedScale <= 200)
        {
            window.setWindowScale(requestedScale);
        }
    }
    window.show();

    // Deterministic packaging check for the same close path used by the
    // title-bar/Alt+F4/context-menu exit actions.
    if (arguments.contains(QStringLiteral("--manual-close-test")))
    {
        QTimer::singleShot(500, &window, [&window]() { window.close(); });
    }

    if (arguments.contains(QStringLiteral("--preview-notices")))
    {
        window.handleCodexEvent(QStringLiteral("thinking"), QStringLiteral("正在理解任务并规划下一步"), {}, {});
        window.handleCodexEvent(QStringLiteral("working"), QStringLiteral("正在调用工具并处理任务"), {}, {});
        window.handleCodexEvent(QStringLiteral("approval"), QStringLiteral("请确认测试操作"), {}, {});
        window.handleCodexEvent(QStringLiteral("completed"), QStringLiteral("测试任务已经完成"), {}, {});
    }
    else if (arguments.contains(QStringLiteral("--preview-persistent")))
    {
        window.handleCodexEvent(QStringLiteral("approval"), QStringLiteral("请确认测试操作"), {}, {});
        window.handleCodexEvent(QStringLiteral("failed"), QStringLiteral("测试任务未能完成"), {}, {});
    }
    else if (arguments.contains(QStringLiteral("--preview-outcome-dedup")))
    {
        window.handleCodexEvent(
            QStringLiteral("completed"), {}, QStringLiteral("primary"), QStringLiteral("turn-1"));
        window.handleCodexEvent(
            QStringLiteral("thinking"), {}, QStringLiteral("secondary"), QStringLiteral("turn-2"));
        window.handleCodexEvent(
            QStringLiteral("idle"), {}, QStringLiteral("secondary"), QStringLiteral("turn-2"));
    }


    const qsizetype aboutSmokeIndex = arguments.indexOf(QStringLiteral("--about-smoke"));
    if (aboutSmokeIndex >= 0 && aboutSmokeIndex + 1 < arguments.size())
    {
        const QString screenshotPath = QDir::cleanPath(arguments.at(aboutSmokeIndex + 1));
        QTimer::singleShot(300, &window, [&window]() { window.showAboutForTesting(); });
        QTimer::singleShot(800, &app, [&app, screenshotPath]() {
            bool saved = false;
            const auto windows = QApplication::topLevelWidgets();
            for (QWidget* widget : windows)
            {
                if (widget->windowTitle() == QStringLiteral("关于 WhaleMaid 桌宠"))
                {
                    saved = widget->grab().save(screenshotPath, "PNG");
                    widget->close();
                    break;
                }
            }
            app.exit(saved ? 0 : 2);
        });
    }

    const qsizetype sequenceIndex = arguments.indexOf(QStringLiteral("--smoke-sequence"));
    if (sequenceIndex >= 0 && sequenceIndex + 1 < arguments.size())
    {
        const QString prefix = QDir::cleanPath(arguments.at(sequenceIndex + 1));
        auto savedFrames = std::make_shared<int>(0);
        for (int frame = 0; frame < 3; ++frame)
        {
            QTimer::singleShot(300 + frame * 600, &app,
                [&window, prefix, savedFrames, frame]() {
                    if (window.grab().save(
                            prefix + QStringLiteral("-%1.png").arg(frame + 1),
                            "PNG"))
                    {
                        ++(*savedFrames);
                    }
                });
        }
        QTimer::singleShot(1700, &app, [&app, savedFrames]() {
            app.exit(*savedFrames == 3 ? 0 : 2);
        });
    }

    const qsizetype smokeTestIndex = arguments.indexOf(QStringLiteral("--smoke-test"));
    if (smokeTestIndex >= 0 && smokeTestIndex + 1 < arguments.size())
    {
        const QString screenshotPath = QDir::cleanPath(arguments.at(smokeTestIndex + 1));
        int smokeDelay = 800;
        const qsizetype smokeDelayIndex = arguments.indexOf(QStringLiteral("--smoke-delay"));
        if (smokeDelayIndex >= 0 && smokeDelayIndex + 1 < arguments.size())
        {
            bool validDelay = false;
            const int requestedDelay = arguments.at(smokeDelayIndex + 1).toInt(&validDelay);
            if (validDelay && requestedDelay >= 0)
            {
                smokeDelay = requestedDelay;
            }
        }
        QTimer::singleShot(smokeDelay, &app, [&app, &window, screenshotPath]() {
            const bool saved = window.grab().save(screenshotPath, "PNG");
            if (saved)
            {
                app.exit(0);
            }
            else
            {
                app.exit(2);
            }
        });
    }

    const qsizetype compositeSmokeIndex = arguments.indexOf(
        QStringLiteral("--composite-smoke"));
    if (compositeSmokeIndex >= 0 && compositeSmokeIndex + 1 < arguments.size())
    {
        const QString screenshotPath = QDir::cleanPath(
            arguments.at(compositeSmokeIndex + 1));
        QTimer::singleShot(1000, &app, [&app, &window, screenshotPath]() {
            app.exit(window.saveCompositeForTesting(screenshotPath) ? 0 : 2);
        });
    }

    return app.exec();
}
