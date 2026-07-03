#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>

// splitmix64: fast, deterministic PRNG used for the random trace
static uint64_t rngState = 0xDEADBEEFCAFEBABEull;
static uint64_t nextRand() {
    rngState += 0x9e3779b97f4a7c15ull;
    uint64_t z = rngState;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static void emit(std::ofstream& f, char op, uint64_t addr) {
    f << op << " 0x" << std::hex << std::setfill('0') << std::setw(8) << addr << "\n";
}

// Two full passes over 512 sequential 64-byte-aligned lines.
// Pass 1 (accesses 1-512):  cold start, all misses.
// Pass 2 (accesses 513-1024): warm cache, all hits.
static void genSequential(const std::string& path) {
    std::ofstream f(path);
    f << "# sequential.trace — 2 passes of 512 lines (64 B stride)\n"
         "# Pass 1: cold start (512 misses); Pass 2: hot (512 hits)\n";
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < 512; ++i) {
            uint64_t a = 0x00010000ULL + static_cast<uint64_t>(i) * 64;
            emit(f, (i % 5 == 4) ? 'W' : 'R', a);
        }
}

// 1024 accesses to pseudo-random 64-byte-aligned addresses within a 1 MiB window.
// Expect high miss rate: poor temporal and spatial locality.
static void genRandom(const std::string& path) {
    std::ofstream f(path);
    f << "# random.trace — 1024 pseudo-random accesses in a 1 MiB window\n"
         "# Expect high miss rate (poor temporal and spatial locality)\n";
    for (int i = 0; i < 1024; ++i) {
        uint64_t a = nextRand() & 0xFFFC0ULL; // 1 MiB aligned to 64 B
        emit(f, (i % 4 == 0) ? 'W' : 'R', a);
    }
}

// Row-major traversal of a 128×128 int (4-byte) matrix.
// Stride = 4 B between consecutive accesses → 16 ints share one 64-byte cache line.
// Expected miss rate ≈ 1/16 = 6.25% (one miss to load each line, 15 subsequent hits).
static void genMatrixRowMajor(const std::string& path) {
    std::ofstream f(path);
    f << "# matrix_row_major.trace — 128x128 int matrix, row-major order\n"
         "# stride=4 B; 16 ints per 64-byte line → good spatial locality\n";
    constexpr uint64_t base = 0x00100000ULL;
    constexpr int N = 128;
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c)
            emit(f, 'R', base + static_cast<uint64_t>(r * N + c) * 4);
}

// Column-major traversal of the same 128×128 int matrix.
// Stride = 128 × 4 = 512 B between consecutive accesses → every access is a new cache line.
// Expected miss rate ≈ 100% (no spatial reuse within any cache line).
static void genMatrixColMajor(const std::string& path) {
    std::ofstream f(path);
    f << "# matrix_col_major.trace — 128x128 int matrix, column-major order\n"
         "# stride=512 B; every access touches a different cache line → poor locality\n";
    constexpr uint64_t base = 0x00100000ULL;
    constexpr int N = 128;
    for (int c = 0; c < N; ++c)
        for (int r = 0; r < N; ++r)
            emit(f, 'R', base + static_cast<uint64_t>(r * N + c) * 4);
}

int main(int argc, char* argv[]) {
    const std::string dir = (argc > 1) ? argv[1] : "traces";
    genSequential    (dir + "/sequential.trace");
    genRandom        (dir + "/random.trace");
    genMatrixRowMajor(dir + "/matrix_row_major.trace");
    genMatrixColMajor(dir + "/matrix_col_major.trace");
    std::printf("Generated 4 trace files in '%s/'\n", dir.c_str());
    return 0;
}
