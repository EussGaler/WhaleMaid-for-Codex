#include "live2d/CubismRuntime.hpp"

#include "live2d/CubismAllocator.hpp"

#include <CubismFramework.hpp>

#include <QByteArray>
#include <QFile>
#include <QLoggingCategory>
#include <QString>

#include <cstring>

namespace
{
CubismAllocator allocator;
Csm::CubismFramework::Option options;
int referenceCount = 0;

void cubismLog(const Csm::csmChar* message)
{
    qInfo().noquote() << "[Live2D]" << QString::fromUtf8(message).trimmed();
}

Csm::csmByte* loadFile(const std::string filePath, Csm::csmSizeInt* outSize)
{
    QFile file(QString::fromUtf8(filePath.c_str()));
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning().noquote() << "Live2D asset could not be read:" << file.fileName();
        *outSize = 0;
        return nullptr;
    }

    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty())
    {
        *outSize = 0;
        return nullptr;
    }

    *outSize = static_cast<Csm::csmSizeInt>(bytes.size());
    auto* result = new Csm::csmByte[*outSize];
    std::memcpy(result, bytes.constData(), static_cast<std::size_t>(*outSize));
    return result;
}

void releaseFile(Csm::csmByte* bytes)
{
    delete[] bytes;
}
}

bool CubismRuntime::acquire()
{
    if (referenceCount > 0)
    {
        ++referenceCount;
        return true;
    }

    options.LogFunction = cubismLog;
    options.LoggingLevel = Csm::CubismFramework::Option::LogLevel_Info;
    options.LoadFileFunction = loadFile;
    options.ReleaseBytesFunction = releaseFile;

    if (!Csm::CubismFramework::StartUp(&allocator, &options))
    {
        qWarning() << "Cubism Framework failed to start.";
        return false;
    }

    Csm::CubismFramework::Initialize();
    referenceCount = 1;
    return true;
}

void CubismRuntime::release()
{
    if (referenceCount <= 0 || --referenceCount > 0)
    {
        return;
    }

    Csm::CubismFramework::Dispose();
    Csm::CubismFramework::CleanUp();
}
