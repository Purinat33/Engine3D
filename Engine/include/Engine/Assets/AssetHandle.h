#pragma once
#include <cstdint>

namespace Engine {

	// Pick ONE width and stick to it everywhere. uint64_t is fine long-term.
	using AssetHandle = uint64_t;

	// Inline so it's safe in headers across translation units
	inline constexpr AssetHandle InvalidAssetHandle = 0;

} // namespace Engine