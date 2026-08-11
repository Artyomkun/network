# Copyright 2026 Artyomkun
# SPDX-License-Identifier: Apache-2.0

add_executable(journal_app
    main.cpp
)

target_link_libraries(journal_app PRIVATE logger)

apply_quality(journal_app)
link_logger_runtime(journal_app)
