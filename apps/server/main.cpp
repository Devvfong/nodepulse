#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nodepulse/config/config.hpp>
#include <nodepulse/server/server.hpp>
#include <nodepulse/utils/logger.hpp>

namespace {

void print_usage(std::string_view program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -h, --help             Show this help message and exit\n"
              << "  -v, --version          Print version information and exit\n"
              << "  -c, --config <path>    Specify path to JSON configuration file\n"
              << "  --validate-config      Validate configuration file and exit\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        std::optional<std::string> config_path;
        bool validate_only = false;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
            if (arg == "-v" || arg == "--version") {
                std::cout << "NodePulse version 0.1.0\n";
                return 0;
            }
            if (arg == "--validate-config") {
                validate_only = true;
            } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg.starts_with("--config=")) {
                config_path = arg.substr(9);
            }
        }

        nodepulse::config::Config config;
        try {
            config = nodepulse::config::Config::load(config_path);
        } catch (const std::exception& ex) {
            std::cerr << "Configuration load error: " << ex.what() << '\n';
            return 1;
        }

        auto validation_errors = config.validate();
        if (!validation_errors.empty()) {
            std::cerr << "Configuration validation failed with " << validation_errors.size()
                      << " error(s):\n";
            for (const auto& err : validation_errors) {
                std::cerr << "  - " << err << '\n';
            }
            return 1;
        }

        if (validate_only) {
            std::cout << "Configuration is valid.\n";
            return 0;
        }

        if (config.security.api_key.empty()) {
            std::cerr << "Server startup rejected: security.api_key cannot be empty. "
                      << "Provide a key via configuration file or NODEPULSE_API_KEY environment "
                         "variable.\n";
            return 1;
        }

        nodepulse::utils::Logger::init(config.server.log_level, config.server.log_format == "json");
        auto logger = nodepulse::utils::Logger::get();
        logger->info("NodePulse server starting (v0.1.0)");

        nodepulse::server::Server server(std::move(config));
        server.setup();
        server.run();

        logger->info("NodePulse server stopped gracefully");
        nodepulse::utils::Logger::shutdown();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown error\n";
        return 1;
    }
}
