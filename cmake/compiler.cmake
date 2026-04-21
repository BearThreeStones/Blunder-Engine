if (MSVC)
    add_compile_options(/MP)

    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )

    # VS 2026 新的调试信息更吃资源，限制一下
    add_compile_options(
        $<$<CONFIG:Debug>:/Z7>
    )
endif()
