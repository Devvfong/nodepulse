#include <cstddef>
#include <string_view>

#include <nodepulse/utils/security.hpp>

namespace nodepulse::utils {

bool constant_time_equals(std::string_view expected, std::string_view candidate) noexcept {
    const size_t exp_len = expected.size();
    const size_t cand_len = candidate.size();

    if (exp_len == 0) {
        return cand_len == 0;
    }

    // Accumulate length difference in result. If lengths match, initial difference is 0;
    // otherwise 1.
    volatile unsigned char result = (exp_len == cand_len) ? 0 : 1;

    // Loop runs for exactly exp_len iterations (proportional only to the expected secret length).
    // When candidate is empty, compare against 0 byte.
    // When candidate is non-empty, use modulo indexing to avoid buffer overflow.
    for (size_t i = 0; i < exp_len; ++i) {
        const unsigned char cand_byte =
            (cand_len > 0) ? static_cast<unsigned char>(candidate[i % cand_len]) : 0;
        const unsigned char exp_byte = static_cast<unsigned char>(expected[i]);
        result |= static_cast<unsigned char>(exp_byte ^ cand_byte);
    }

    return result == 0;
}

}  // namespace nodepulse::utils
