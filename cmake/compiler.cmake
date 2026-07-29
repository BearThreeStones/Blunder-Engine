if (MSVC)
    # Cap parallel cl.exe workers. engine_runtime Debug PCH is ~270MB; each worker
    # maps it. Unbounded /MP + low commit free -> C3859 / C1076.
    # Override: cmake -DBLUNDER_CL_MP_COUNT=2
    set(BLUNDER_CL_MP_COUNT "1" CACHE STRING
        "MSVC parallel cl.exe workers (CL_MPCount). Use 1 if C3859/C1076.")

    # Visual Studio generator turns any /MPn into
    #   <MultiProcessorCompilation>true</MultiProcessorCompilation>
    # with NO worker cap (all cores). Never pass /MP* under that generator ��
    # set CL_MPCount via VS globals / target properties instead.
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set(CMAKE_VS_GLOBALS "CL_MPCount=${BLUNDER_CL_MP_COUNT}")
        message(STATUS
            "[MSVC] VS: CL_MPCount=${BLUNDER_CL_MP_COUNT} via CMAKE_VS_GLOBALS")
    elseif(NOT BLUNDER_CL_MP_COUNT STREQUAL "" AND NOT BLUNDER_CL_MP_COUNT STREQUAL "0")
        add_compile_options(/MP${BLUNDER_CL_MP_COUNT})
        message(STATUS "[MSVC] /MP${BLUNDER_CL_MP_COUNT}")
    else()
        message(STATUS "[MSVC] /MP disabled")
    endif()

    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )

    # Embed debug info in .obj (avoids parallel PDB lock).
    add_compile_options(
        $<$<CONFIG:Debug>:/Z7>
    )
endif()

# Call after add_library/add_executable for MSVC VS targets that use the big PCH.
function(blunder_msvc_limit_cl_mp target_name)
    if(NOT MSVC)
        return()
    endif()
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "blunder_msvc_limit_cl_mp: unknown target ${target_name}")
    endif()
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(TARGET "${target_name}" PROPERTY
            VS_GLOBAL_CL_MPCount "${BLUNDER_CL_MP_COUNT}")
    endif()
endfunction()
