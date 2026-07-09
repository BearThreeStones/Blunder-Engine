enable_testing()

add_executable(translate_modal_session_tests
    "translate_modal_session_tests.cpp"
)

target_link_libraries(translate_modal_session_tests
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(translate_modal_session_tests PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(translate_modal_session_tests
    REUSE_FROM engine_runtime
)

add_test(NAME translate_modal_session_tests
         COMMAND translate_modal_session_tests)
