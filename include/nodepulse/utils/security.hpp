#pragma once

#include <string_view>

namespace nodepulse::utils {

/**
 * @brief Constant-time string comparison to prevent timing side-channel attacks.
 *
 * Compares two string views in execution time proportional only to the length
 * of the expected secret string, without early loop exits or branching on
 * secret characters.
 *
 * @param expected The expected reference secret key.
 * @param candidate The candidate key provided in the request.
 * @return true if candidate exactly matches expected, false otherwise.
 */
[[nodiscard]] bool constant_time_equals(std::string_view expected,
                                        std::string_view candidate) noexcept;

}  // namespace nodepulse::utils
