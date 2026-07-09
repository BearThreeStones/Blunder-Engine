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

add_executable(outline_color_tests
    "outline_color_tests.cpp"
)

target_link_libraries(outline_color_tests
    PRIVATE
)

target_include_directories(outline_color_tests
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(outline_color_tests PRIVATE /Zc:preprocessor)
endif()

add_test(NAME outline_color_tests
         COMMAND outline_color_tests)

add_executable(outline_aa_test
    "outline_aa_test.cpp"
    "../runtime/function/render/overlay/outline_aa.cpp"
)

target_include_directories(outline_aa_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(outline_aa_test PRIVATE /Zc:preprocessor)
endif()

add_test(NAME outline_aa_test
         COMMAND outline_aa_test)
