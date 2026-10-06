#include <memory>
#include <string>
#include <utility>

#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>

#include <nodepulse/controllers/events_controller.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::StreamService> EventsController::stream_service_ = nullptr;

std::shared_ptr<services::StreamService> EventsController::get_stream_service() {
    return stream_service_;
}

void EventsController::get_events(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }
    if (req_id.empty() && req) {
        req_id = req->getHeader("X-Request-ID");
        if (req_id.empty()) {
            req_id = req->getHeader("x-request-id");
        }
    }
    if (req_id.empty()) {
        req_id = drogon::utils::getUuid();
    }

    auto stream_svc = get_stream_service();
    if (!stream_svc || !stream_svc->is_enabled()) {
        auto resp = utils::make_error_response(
            drogon::k503ServiceUnavailable, utils::error_codes::kServiceUnavailable,
            "Server-Sent Events streaming is disabled by configuration.", nlohmann::json::array(),
            req_id);
        callback(resp);
        return;
    }

    auto reservation = stream_svc->try_reserve_slot();
    if (!reservation) {
        auto resp = utils::make_error_response(
            drogon::k503ServiceUnavailable, utils::error_codes::kServiceUnavailable,
            "SSE connection capacity is currently exhausted.", nlohmann::json::array(), req_id);
        callback(resp);
        return;
    }

    auto resp = drogon::HttpResponse::newAsyncStreamResponse(
        [res = std::make_shared<std::unique_ptr<services::SseSlotReservation>>(
             std::move(reservation))](drogon::ResponseStreamPtr stream) {
            auto client = std::make_shared<services::DrogonSseClient>(std::move(stream));
            if (res && *res) {
                (*res)->commit(std::move(client));
            }
        },
        true /* disableKickoffTimeout */
    );

    resp->setContentTypeCodeAndCustomString(drogon::CT_CUSTOM, "text/event-stream; charset=utf-8");
    resp->addHeader("Cache-Control", "no-cache");
    resp->addHeader("Connection", "keep-alive");
    resp->addHeader("X-Request-ID", req_id);

    callback(resp);
}

}  // namespace nodepulse::controllers
