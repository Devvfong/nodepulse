#include <cstddef>
#include <string>
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

std::string redact_connection_string(std::string_view conn_str) {
    std::string s(conn_str);

    // 1. Redact URI style: <proto>://<user>:<password>@<host>...
    auto proto_pos = s.find("://");
    if (proto_pos != std::string::npos) {
        size_t auth_start = proto_pos + 3;
        size_t path_pos = s.find_first_of("/?#", auth_start);
        size_t search_limit = (path_pos != std::string::npos) ? path_pos : s.size();
        size_t at_pos = s.rfind('@', search_limit);
        if (at_pos != std::string::npos && at_pos >= auth_start &&
            (path_pos == std::string::npos || at_pos < path_pos)) {
            size_t colon_pos = s.find(':', auth_start);
            if (colon_pos != std::string::npos && colon_pos < at_pos) {
                s.replace(colon_pos + 1, at_pos - colon_pos - 1, "***");
            }
        }
    }

    // 2. Redact key-value / query params: password=<secret> or password='<secret>'
    // Case-insensitive scan for "password"
    const std::string target = "password";
    size_t search_start = 0;
    while (search_start < s.size()) {
        // Find 'password' ignoring case
        size_t match_pos = std::string::npos;
        for (size_t i = search_start; i + target.size() <= s.size(); ++i) {
            bool matches = true;
            for (size_t j = 0; j < target.size(); ++j) {
                char c1 = s[i + j];
                char c2 = target[j];
                if (c1 >= 'A' && c1 <= 'Z') {
                    c1 = static_cast<char>(c1 + ('a' - 'A'));
                }
                if (c1 != c2) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                // Check word boundary preceding: start of string or whitespace or '&' or ';' or ','
                if (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t' || s[i - 1] == '\n' ||
                    s[i - 1] == '&' || s[i - 1] == ';' || s[i - 1] == ',') {
                    match_pos = i;
                    break;
                }
            }
        }

        if (match_pos == std::string::npos) {
            break;
        }

        size_t eq_pos = match_pos + target.size();
        // Skip whitespace between 'password' and '='
        while (eq_pos < s.size() && (s[eq_pos] == ' ' || s[eq_pos] == '\t')) {
            ++eq_pos;
        }

        if (eq_pos < s.size() && s[eq_pos] == '=') {
            size_t val_start = eq_pos + 1;
            while (val_start < s.size() && (s[val_start] == ' ' || s[val_start] == '\t')) {
                ++val_start;
            }

            if (val_start < s.size()) {
                size_t val_end = val_start;
                char quote = s[val_start];
                if (quote == '\'' || quote == '"') {
                    // Quoted value
                    val_end = s.find(quote, val_start + 1);
                    if (val_end != std::string::npos) {
                        ++val_end;  // include quote in replacement
                    } else {
                        val_end = s.size();
                    }
                } else {
                    // Unquoted value until whitespace, '&', ';', ',', or end
                    while (val_end < s.size() && s[val_end] != ' ' && s[val_end] != '\t' &&
                           s[val_end] != '\n' && s[val_end] != '&' && s[val_end] != ';' &&
                           s[val_end] != ',') {
                        ++val_end;
                    }
                }

                s.replace(val_start, val_end - val_start, "***");
                search_start = val_start + 3;
            } else {
                search_start = eq_pos + 1;
            }
        } else {
            search_start = match_pos + target.size();
        }
    }

    return s;
}

}  // namespace nodepulse::utils
