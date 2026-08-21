#pragma once

class CubismRuntime final
{
public:
    static bool acquire();
    static void release();

private:
    CubismRuntime() = delete;
};
