#include "dataset.h"
#include "kmeans_openmp.h"
#include "kmeans_serial.h"
#include "timer.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct ProgramOptions {
    std::size_t n = 10000;
    std::size_t dim = 2;
    std::size_t k = 3;
    int max_iter = 100;
    double tol = 1e-4;
    int threads = 4;
    std::string mode = "parallel";
    std::uint32_t seed = 42;
};

void print_help(const char *program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Options:\n"
        << "  --n <value>          样本数量，默认 10000\n"
        << "  --dim <value>        特征维度，默认 2\n"
        << "  --k <value>          聚类数量，默认 3\n"
        << "  --max_iter <value>   最大迭代次数，默认 100\n"
        << "  --tol <value>        收敛阈值，默认 1e-4\n"
        << "  --threads <value>    OpenMP 线程数，默认 4\n"
        << "  --mode <serial|parallel> 运行模式，默认 parallel\n"
        << "  --seed <value>       随机种子，默认 42\n"
        << "  --help               显示帮助信息\n\n"
        << "Output CSV columns:\n"
        << "mode,n,dim,k,max_iter,threads,iterations,time,inertia\n";
}

std::string require_value(int &i, int argc, char *argv[]) {
    if (i + 1 >= argc) {
        throw std::invalid_argument(std::string("缺少参数值: ") + argv[i]);
    }
    ++i;
    return argv[i];
}

ProgramOptions parse_args(int argc, char *argv[]) {
    ProgramOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            std::exit(0);
        } else if (arg == "--n") {
            options.n = static_cast<std::size_t>(std::stoull(require_value(i, argc, argv)));
        } else if (arg == "--dim") {
            options.dim = static_cast<std::size_t>(std::stoull(require_value(i, argc, argv)));
        } else if (arg == "--k") {
            options.k = static_cast<std::size_t>(std::stoull(require_value(i, argc, argv)));
        } else if (arg == "--max_iter") {
            options.max_iter = std::stoi(require_value(i, argc, argv));
        } else if (arg == "--tol") {
            options.tol = std::stod(require_value(i, argc, argv));
        } else if (arg == "--threads") {
            options.threads = std::stoi(require_value(i, argc, argv));
        } else if (arg == "--mode") {
            options.mode = require_value(i, argc, argv);
        } else if (arg == "--seed") {
            options.seed = static_cast<std::uint32_t>(std::stoul(require_value(i, argc, argv)));
        } else {
            throw std::invalid_argument("未知参数: " + arg);
        }
    }

    if (options.n == 0 || options.dim == 0 || options.k == 0) {
        throw std::invalid_argument("n、dim 和 k 必须大于 0");
    }
    if (options.k > options.n) {
        throw std::invalid_argument("k 不能大于 n");
    }
    if (options.max_iter <= 0 || options.tol < 0.0 || options.threads <= 0) {
        throw std::invalid_argument("max_iter 和 threads 必须大于 0，tol 必须大于等于 0");
    }
    if (options.mode != "serial" && options.mode != "parallel") {
        throw std::invalid_argument("mode 只能是 serial 或 parallel");
    }

    return options;
}

} // namespace

int main(int argc, char *argv[]) {
    try {
        const ProgramOptions options = parse_args(argc, argv);
        const Dataset data = generate_random_dataset(options.n, options.dim, options.k, options.seed);

        KMeansConfig config;
        config.k = options.k;
        config.max_iter = options.max_iter;
        config.tol = options.tol;
        config.threads = options.threads;

        Timer timer;
        KMeansResult result;
        if (options.mode == "serial") {
            result = kmeans_serial(data, config);
        } else {
            result = kmeans_openmp(data, config);
        }
        const double elapsed = timer.elapsed_seconds();

        std::cout << options.mode << ','
                  << options.n << ','
                  << options.dim << ','
                  << options.k << ','
                  << options.max_iter << ','
                  << options.threads << ','
                  << result.iterations << ','
                  << std::fixed << std::setprecision(9) << elapsed << ','
                  << std::setprecision(6) << result.inertia << '\n';
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        std::cerr << "Use --help to see usage.\n";
        return 1;
    }

    return 0;
}
