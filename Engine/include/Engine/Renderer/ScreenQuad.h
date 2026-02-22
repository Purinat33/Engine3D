#pragma once
#include <memory>

namespace Engine {

    class VertexArray;

    class ScreenQuad {
    public:
        static std::shared_ptr<VertexArray> GetVAO();
    };

} // namespace Engine