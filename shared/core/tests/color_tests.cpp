#include "test.hpp"
#include "core/math/color.hpp"

#include <cmath>

TEST_CASE(srgb_conversion_matches_the_standard_and_round_trips) {
    CHECK(srgbToLinear(0.0f) == 0.0f);
    CHECK(std::abs(srgbToLinear(1.0f) - 1.0f) < 1e-6f);
    // Mid grey: sRGB 0.5 is about 0.214 linear; the linear segment near black
    CHECK(std::abs(srgbToLinear(0.5f) - 0.21404f) < 1e-4f);
    CHECK(std::abs(srgbToLinear(0.02f) - 0.02f / 12.92f) < 1e-7f);

    bool roundTrips = true;
    for (int i = 0; i <= 255; ++i) {
        const f32 value = static_cast<f32>(i) / 255.0f;
        roundTrips = roundTrips && std::abs(linearToSrgb(srgbToLinear(value)) - value) < 1e-5f;
    }
    CHECK(roundTrips);

    const Vec3 converted = srgbToLinear(Vec3(1.0f, 0.5f, 0.0f));
    CHECK(std::abs(converted.y - 0.21404f) < 1e-4f && converted.z == 0.0f);
}
