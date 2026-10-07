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

/**
 * @brief Redacts sensitive credentials (such as passwords) from database connection strings
 *        in both URI format (postgresql://user:pass@host/db) and key-value format (password=pass).
 *
 * @param conn_str The database connection string or URI.
 * @return Redacted connection string safe for logging and error reporting.
 */
[[nodiscard]] std::string redact_connection_string(std::string_view conn_str);

}  // namespace nodepulse::utils
