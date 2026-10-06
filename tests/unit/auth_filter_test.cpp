#include <string>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/middleware/auth_filter.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::middleware {

class AuthFilterTest : public ::testing::Test {
  protected:
    void SetUp() override {
        AuthFilter::set_api_key("np_test_secret_phase9_valid_key");
    }

    void TearDown() override {
        AuthFilter::reset();
    }
};

TEST_F(AuthFilterTest, HealthEndpointIsExempt) {
    EXPECT_TRUE(AuthFilter::is_exempt_path("/api/v1/health"));
    EXPECT_TRUE(AuthFilter::is_exempt_path("/api/v1/health/"));
    EXPECT_TRUE(AuthFilter::is_exempt_path("/api/v1/health?verbose=1"));

    // Paths resembling /health must NOT be exempt
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/healthy"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/healthcheck"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/health/status"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/health1"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/health"));
}

TEST_F(AuthFilterTest, OperationalEndpointsAreNotExempt) {
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/system"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/cpu"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/memory"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/disks"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/network"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/processes"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/processes/1"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/services"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/api/v1/services/cron.service"));
    EXPECT_FALSE(AuthFilter::is_exempt_path("/"));
}

TEST_F(AuthFilterTest, AuthenticateRejectsNullRequest) {
    EXPECT_FALSE(AuthFilter::authenticate(nullptr));
}

TEST_F(AuthFilterTest, AuthenticateRejectsWhenNoKeyConfigured) {
    AuthFilter::reset();
    auto req = drogon::HttpRequest::newHttpRequest();
    req->addHeader("X-API-Key", "np_test_secret_phase9_valid_key");
    EXPECT_FALSE(AuthFilter::authenticate(req));
}

TEST_F(AuthFilterTest, AuthenticateAcceptsValidKey) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->addHeader("X-API-Key", "np_test_secret_phase9_valid_key");
    EXPECT_TRUE(AuthFilter::authenticate(req));
}

TEST_F(AuthFilterTest, AuthenticateAcceptsValidKeyCaseInsensitive) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->addHeader("x-api-key", "np_test_secret_phase9_valid_key");
    EXPECT_TRUE(AuthFilter::authenticate(req));
}

TEST_F(AuthFilterTest, AuthenticateRejectsInvalidKey) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->addHeader("X-API-Key", "wrong_key_12345");
    EXPECT_FALSE(AuthFilter::authenticate(req));
}

TEST_F(AuthFilterTest, AuthenticateRejectsEmptyKey) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->addHeader("X-API-Key", "");
    EXPECT_FALSE(AuthFilter::authenticate(req));
}

TEST_F(AuthFilterTest, AuthenticateRejectsMissingHeader) {
    auto req = drogon::HttpRequest::newHttpRequest();
    EXPECT_FALSE(AuthFilter::authenticate(req));
}

TEST_F(AuthFilterTest, HandleRequestAllowsExemptPath) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/health");

    bool chain_called = false;
    bool advice_called = false;

    AuthFilter::handle_request(
        req, [&advice_called](const drogon::HttpResponsePtr&) { advice_called = true; },
        [&chain_called]() { chain_called = true; });

    EXPECT_TRUE(chain_called);
    EXPECT_FALSE(advice_called);
}

TEST_F(AuthFilterTest, HandleRequestRejectsProtectedEndpointWithoutKey) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/cpu");
    req->getAttributes()->insert("request_id", std::string("test-req-id-uuid"));

    bool chain_called = false;
    drogon::HttpResponsePtr intercepted_resp;

    AuthFilter::handle_request(
        req, [&intercepted_resp](const drogon::HttpResponsePtr& resp) { intercepted_resp = resp; },
        [&chain_called]() { chain_called = true; });

    EXPECT_FALSE(chain_called);
    ASSERT_NE(intercepted_resp, nullptr);
    EXPECT_EQ(intercepted_resp->statusCode(), drogon::k401Unauthorized);
    EXPECT_EQ(intercepted_resp->contentType(), drogon::CT_APPLICATION_JSON);
    EXPECT_EQ(intercepted_resp->getHeader("X-Request-ID"), "test-req-id-uuid");

    auto json = nlohmann::json::parse(intercepted_resp->getBody());
    ASSERT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"]["code"], "UNAUTHORIZED");
    EXPECT_FALSE(json["error"]["message"].get<std::string>().empty());
}

TEST_F(AuthFilterTest, HandleRequestApprovesValidKey) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/system");
    req->addHeader("X-API-Key", "np_test_secret_phase9_valid_key");

    bool chain_called = false;
    bool advice_called = false;

    AuthFilter::handle_request(
        req, [&advice_called](const drogon::HttpResponsePtr&) { advice_called = true; },
        [&chain_called]() { chain_called = true; });

    EXPECT_TRUE(chain_called);
    EXPECT_FALSE(advice_called);
    ASSERT_NE(req->getAttributes(), nullptr);
    EXPECT_TRUE(req->getAttributes()->find("is_authenticated"));
    EXPECT_TRUE(req->getAttributes()->get<bool>("is_authenticated"));
}

}  // namespace nodepulse::middleware
