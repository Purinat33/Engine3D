#pragma once
#include <cstdint>

namespace Engine {

    class RenderCommand {
    public:
        static void Init();
        static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        static void SetClearColor(float r, float g, float b, float a);
        static void Clear();
        static void DrawIndexed(uint32_t indexCount);
    };

} // namespace Engine