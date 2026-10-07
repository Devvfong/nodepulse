#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <drogon/HttpController.h>

#include <nodepulse/services/stream_service.hpp>

namespace nodepulse::controllers {

class EventsController : public drogon::HttpController<EventsController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(EventsController::get_events, "/api/v1/events", drogon::Get);
    METHOD_LIST_END

    void get_events(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    static void set_stream_service(std::shared_ptr<services::StreamService> service);

    [[nodiscard]] static std::shared_ptr<services::StreamService> get_stream_service();

  private:
    static std::shared_ptr<services::StreamService> stream_service_;
    static std::mutex mutex_;
};

}  // namespace nodepulse::controllers
