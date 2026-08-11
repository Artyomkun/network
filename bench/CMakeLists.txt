# Micro-benchmarks: write latency, formatter comparison across standard
# versions, multithreading, file vs socket.
#
# Run: ./bench_logger (a Release build gives meaningful numbers).
add_executable(bench_logger
    bench_logger.cpp
)

target_link_libraries(bench_logger PRIVATE logger)

apply_quality(bench_logger)