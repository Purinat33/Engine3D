#include "pch.h"
#include "Engine/Scene/UUID.h"

#include <random>

namespace Engine {

    UUID GenerateUUID() {
        static std::mt19937_64 rng{ std::random_device{}() };
        static std::uniform_int_distribution<UUID> dist;
        UUID id = dist(rng);
        if (id == 0) id = dist(rng); // avoid InvalidAssetHandle-like 0
        return id;
    }

} // namespace Engine