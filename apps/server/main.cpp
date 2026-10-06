#include <iostream>
#include <string_view>

#include <drogon/drogon.h>
#include <nlohmann/json.hpp>

#include <nodepulse/utils/logger.hpp>

int main(int argc, char* argv[]) {
    try {
        nodepulse::utils::Logger::init("info");
        auto logger = nodepulse::utils::Logger::get();

        const nlohmann::json build_metadata = {{"project", "NodePulse"},
                                               {"version", "0.1.0"},
                                               {"phase", "Phase 1 - Repository Foundation"},
                                               {"drogon_version", drogon::getVersion()}};

        logger->info("NodePulse foundation initialized: {}", build_metadata.dump());

        if (argc > 1 && std::string_view(argv[1]) == "--version") {
            std::cout << "NodePulse version 0.1.0 (Phase 1 Foundation)\n";
            return 0;
        }

        logger->info("Build and linkage verification complete. Ready for Phase 2.");
        nodepulse::utils::Logger::shutdown();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error during startup: " << ex.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error during startup\n";
        return 1;
    }
}
