add_executable(journal_app
    main.cpp
)

target_link_libraries(journal_app PRIVATE logger)

apply_quality(journal_app)
