#include "input.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: mcr_reconstruct --image input.png --camera camera.json --output reconstruction.json\n"
                     "Known unit 2x2x2 grid, exact first-hit RGB/opaque RGBA, supported A2 palettes.\n"
                     "Exit 0: complete SAT/UNSAT result. Exit 2: invalid input/arguments. Exit 3: execution/output error.\n";
        return 0;
    }
    try {
        std::map<std::string,std::filesystem::path> args;
        for (int i = 1; i < argc; i += 2) {
            const std::string flag(argv[i]);
            if (flag != "--image" && flag != "--camera" && flag != "--output") throw std::invalid_argument("unknown argument: " + flag);
            if (i+1 >= argc || std::string(argv[i+1]).empty() || !args.emplace(flag,argv[i+1]).second)
                throw std::invalid_argument("missing value or duplicate argument: " + flag);
        }
        if (args.size() != 3) throw std::invalid_argument("required: --image PNG --camera JSON --output JSON (see --help)");
        const auto output = std::filesystem::weakly_canonical(args.at("--output"));
        for (const auto* key : {"--image","--camera"}) {
            const auto source = std::filesystem::weakly_canonical(args.at(key));
            if (source == output || (std::filesystem::exists(source) && std::filesystem::exists(output) && std::filesystem::equivalent(source,output)))
                throw std::invalid_argument("output must not overwrite an input file");
        }
        const auto input = mcr::app::read_metadata(args.at("--camera"));
        const auto pixels = mcr::app::read_png(args.at("--image"),input);
        const auto result = mcr::app::reconstruct(input,pixels);
        mcr::app::write_result(args.at("--output"),result);
        std::cout << result.get<mcr::app::Object>().at("feasibility").get<std::string>() << '\n';
        return 0;
    } catch (const std::invalid_argument& error) {
        std::cerr << "mcr_reconstruct: " << error.what() << '\n'; return 2;
    } catch (const std::exception& error) {
        std::cerr << "mcr_reconstruct: " << error.what() << '\n'; return 3;
    }
}
