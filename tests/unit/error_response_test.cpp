#include <string>

#include <drogon/HttpResponse.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::utils {

TEST(ErrorResponseTest, GeneratesStandardErrorJson) {
    nlohmann::json details =
        nlohmann::json::array({{{"field", "pid"}, {"reason", "must_be_positive_integer"}}});

    auto err_json =
        make_error_json(error_codes::kInvalidRequest, "Process ID must be a positive integer.",
                        details, "2026-10-06T12:00:00Z");

    ASSERT_TRUE(err_json.contains("error"));
    const auto& err_obj = err_json["error"];
    EXPECT_EQ(err_obj["code"], "INVALID_REQUEST");
    EXPECT_EQ(err_obj["message"], "Process ID must be a positive integer.");
    EXPECT_EQ(err_obj["timestamp"], "2026-10-06T12:00:00Z");
    ASSERT_TRUE(err_obj["details"].is_array());
    EXPECT_EQ(err_obj["details"].size(), 1);
    EXPECT_EQ(err_obj["details"][0]["field"], "pid");
}

TEST(ErrorResponseTest, GeneratesDynamicTimestampWhenEmpty) {
    auto err_json = make_error_json(error_codes::kResourceNotFound, "Resource not found");

    ASSERT_TRUE(err_json.contains("error"));
    const auto& err_obj = err_json["error"];
    EXPECT_EQ(err_obj["code"], "RESOURCE_NOT_FOUND");
    EXPECT_FALSE(err_obj["timestamp"].get<std::string>().empty());
    EXPECT_TRUE(err_obj["details"].is_array());
    EXPECT_TRUE(err_obj["details"].empty());
}

TEST(ErrorResponseTest, CreatesHttpResponseWithHeaders) {
    auto resp = make_error_response(drogon::k404NotFound, error_codes::kResourceNotFound,
                                    "Not Found", nlohmann::json::array(), "req-id-uuid-12345");

    EXPECT_EQ(resp->statusCode(), drogon::k404NotFound);
    EXPECT_EQ(resp->getHeader("Content-Type"), "application/json");
    EXPECT_EQ(resp->getHeader("X-Request-ID"), "req-id-uuid-12345");

    auto parsed = nlohmann::json::parse(resp->getBody());
    EXPECT_TRUE(parsed.contains("error"));
    EXPECT_EQ(parsed["error"]["code"], "RESOURCE_NOT_FOUND");
}

}  // namespace nodepulse::utils
