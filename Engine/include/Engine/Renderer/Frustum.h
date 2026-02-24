#pragma once
#include <glm/glm.hpp>

namespace Engine {

    struct Frustum {
        // plane: ax + by + cz + d = 0
        glm::vec4 Planes[6];
    };

    inline Frustum ExtractFrustum(const glm::mat4& vp) {
        Frustum f{};

        // Column-major glm: vp[col][row]
        const glm::vec4 c0 = vp[0];
        const glm::vec4 c1 = vp[1];
        const glm::vec4 c2 = vp[2];
        const glm::vec4 c3 = vp[3];

        // rows = transpose cols
        glm::vec4 r0(c0.x, c1.x, c2.x, c3.x);
        glm::vec4 r1(c0.y, c1.y, c2.y, c3.y);
        glm::vec4 r2(c0.z, c1.z, c2.z, c3.z);
        glm::vec4 r3(c0.w, c1.w, c2.w, c3.w);

        // Left/Right/Bottom/Top/Near/Far
        f.Planes[0] = r3 + r0;
        f.Planes[1] = r3 - r0;
        f.Planes[2] = r3 + r1;
        f.Planes[3] = r3 - r1;
        f.Planes[4] = r3 + r2;
        f.Planes[5] = r3 - r2;

        // Normalize
        for (auto& p : f.Planes) {
            glm::vec3 n(p.x, p.y, p.z);
            float len = glm::length(n);
            if (len > 0.00001f) p /= len;
        }
        return f;
    }

    inline bool SphereInFrustum(const Frustum& f, const glm::vec3& c, float r) {
        for (const auto& p : f.Planes) {
            float d = p.x * c.x + p.y * c.y + p.z * c.z + p.w;
            if (d < -r) return false;
        }
        return true;
    }

} // namespace Engine