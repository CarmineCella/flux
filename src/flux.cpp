#include "flux.h"
#include "dsp.h"

int main(int argc, char* argv[]) {
    flux::Interpreter interp;
    flux::register_dsp(interp);

    std::vector<std::string> files;
    flux::List flux_argv;
    bool enter_repl = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--i" || arg == "-i") {
            enter_repl = true;
        } else if ((arg == "--stack" || arg == "-s") && i + 1 < argc) {
            interp.max_stack = std::stoi(argv[++i]);
        } else if (arg == "--args") {
            for (++i; i < argc; ++i)
                flux_argv.push_back(flux::Value(flux::Str(argv[i])));
            break;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: flux [options] [files...]\n"
                      << "  --i          enter REPL after running files\n"
                      << "  --stack n    set max recursion depth (default: 1000)\n"
                      << "  --args ...   pass remaining arguments as __argv\n"
                      << "  --help       show this help\n";
            return 0;
        } else {
            files.push_back(arg);
        }
    }

    interp.global->def("__argv", flux::Value(flux_argv));

    for (auto& f : files) {
        try { interp.run_file(f); }
        catch (std::exception& e) {
            std::cerr << "error: " << e.what() << '\n';
            return 1;
        }
    }

    if (files.empty() || enter_repl)
        interp.repl();

    return 0;
}
