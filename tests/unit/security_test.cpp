#include <string>

#include <gtest/gtest.h>

#include <nodepulse/utils/security.hpp>

namespace nodepulse::utils {

TEST(SecurityTest, ConstantTimeEqualsExactMatch) {
    EXPECT_TRUE(constant_time_equals("np_live_secret_key_12345", "np_live_secret_key_12345"));
    EXPECT_TRUE(constant_time_equals("a", "a"));
    EXPECT_TRUE(constant_time_equals("1234567890abcdef", "1234567890abcdef"));
}

TEST(SecurityTest, ConstantTimeEqualsBothEmpty) {
    EXPECT_TRUE(constant_time_equals("", ""));
}

TEST(SecurityTest, ConstantTimeEqualsEmptyExpectedOrCandidate) {
    EXPECT_FALSE(constant_time_equals("", "candidate"));
    EXPECT_FALSE(constant_time_equals("expected", ""));
}

TEST(SecurityTest, ConstantTimeEqualsSingleByteMismatchAtStart) {
    EXPECT_FALSE(constant_time_equals("np_secret_12345", "xp_secret_12345"));
}

TEST(SecurityTest, ConstantTimeEqualsSingleByteMismatchInMiddle) {
    EXPECT_FALSE(constant_time_equals("np_secret_12345", "np_secXet_12345"));
}

TEST(SecurityTest, ConstantTimeEqualsSingleByteMismatchAtEnd) {
    EXPECT_FALSE(constant_time_equals("np_secret_12345", "np_secret_12346"));
}

TEST(SecurityTest, ConstantTimeEqualsLengthMismatchShorter) {
    EXPECT_FALSE(constant_time_equals("np_secret_12345", "np_secret_1234"));
    EXPECT_FALSE(constant_time_equals("np_secret_12345", "n"));
}

TEST(SecurityTest, ConstantTimeEqualsLengthMismatchLonger) {
    EXPECT_FALSE(constant_time_equals("np_secret_12345", "np_secret_123456"));
    EXPECT_FALSE(constant_time_equals("np_secret_12345", "np_secret_12345_extra_suffix"));
}

TEST(SecurityTest, ConstantTimeEqualsCaseSensitive) {
    EXPECT_FALSE(constant_time_equals("np_secret_key", "NP_SECRET_KEY"));
    EXPECT_FALSE(constant_time_equals("np_Secret_Key", "np_secret_key"));
}

TEST(SecurityTest, ConstantTimeEqualsBinaryDataWithNullBytes) {
    std::string key_with_null = std::string("pre\0fix", 7);
    std::string candidate_match = std::string("pre\0fix", 7);
    std::string candidate_diff = std::string("pre\0fiX", 7);

    EXPECT_TRUE(constant_time_equals(key_with_null, candidate_match));
    EXPECT_FALSE(constant_time_equals(key_with_null, candidate_diff));
}

TEST(SecurityTest, ConstantTimeEqualsOversizedCandidateDoesNotCrashOrLeak) {
    std::string expected = "np_secret_key_phase9";
    std::string huge_candidate(50000, 'A');

    EXPECT_FALSE(constant_time_equals(expected, huge_candidate));
}

TEST(SecurityTest, RedactConnectionStringUriWithPassword) {
    EXPECT_EQ(
        redact_connection_string("postgresql://nodepulse:secret123@localhost:5432/nodepulse_db"),
        "postgresql://nodepulse:***@localhost:5432/nodepulse_db");
    EXPECT_EQ(redact_connection_string("postgres://user:p@ssw0rd@db.internal/mydb?sslmode=require"),
              "postgres://user:***@db.internal/mydb?sslmode=require");
}

TEST(SecurityTest, RedactConnectionStringUriWithoutPassword) {
    EXPECT_EQ(redact_connection_string("postgresql://nodepulse@localhost:5432/nodepulse_db"),
              "postgresql://nodepulse@localhost:5432/nodepulse_db");
    EXPECT_EQ(redact_connection_string("postgresql://localhost:5432/nodepulse_db"),
              "postgresql://localhost:5432/nodepulse_db");
}

TEST(SecurityTest, RedactConnectionStringAlreadyRedacted) {
    EXPECT_EQ(redact_connection_string("postgresql://nodepulse:***@localhost:5432/nodepulse_db"),
              "postgresql://nodepulse:***@localhost:5432/nodepulse_db");
    EXPECT_EQ(redact_connection_string("host=localhost password=*** dbname=db"),
              "host=localhost password=*** dbname=db");
}

TEST(SecurityTest, RedactConnectionStringKeyValueFormats) {
    EXPECT_EQ(redact_connection_string(
                  "host=localhost port=5432 user=nodepulse password=secret123 dbname=np"),
              "host=localhost port=5432 user=nodepulse password=*** dbname=np");
    EXPECT_EQ(redact_connection_string("host=localhost password='quoted_secret' dbname=np"),
              "host=localhost password=*** dbname=np");
    EXPECT_EQ(redact_connection_string("host=localhost password=\"double_quoted\" dbname=np"),
              "host=localhost password=*** dbname=np");
    EXPECT_EQ(redact_connection_string("PASSWORD=UPPERCASE_SECRET host=localhost"),
              "PASSWORD=*** host=localhost");
}

TEST(SecurityTest, RedactConnectionStringQueryParamFormat) {
    EXPECT_EQ(
        redact_connection_string("postgresql://host/db?user=app&password=secret&sslmode=disable"),
        "postgresql://host/db?user=app&password=***&sslmode=disable");
}

TEST(SecurityTest, RedactConnectionStringEmpty) {
    EXPECT_EQ(redact_connection_string(""), "");
}

}  // namespace nodepulse::utils
