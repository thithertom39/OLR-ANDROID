add_library(xrImGui STATIC)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/Externals/imgui")

target_sources(xrImGui PRIVATE
    "${IMGUI_DIR}/imconfig.h"
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui.h"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_internal.h"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.h"
    "${IMGUI_DIR}/imstb_rectpack.h"
    "${IMGUI_DIR}/imstb_textedit.h"
    "${IMGUI_DIR}/imstb_truetype.h"
)

target_include_directories(xrImGui
    PUBLIC
    "${IMGUI_DIR}"
    PRIVATE
    "${CMAKE_SOURCE_DIR}/sdk/include"
)

# On Android the OpenGL backend uses xrRender_GL's GLAD dispatch table.
# Keep this archive free of direct system GLES/EGL dependencies so that
# SDL can select either the system implementation or app-local ANGLE.
if (ANDROID)
    # Force GLAD first only for this backend translation unit. Its GLES header
    # guards prevent ImGui's subsequent system GLES include from declaring a
    # second/direct API. The function-pointer definitions remain owned once by
    # xrRender_GL; no GLAD implementation or loader is added to this archive.
    set_property(SOURCE "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
        "-include" "${CMAKE_SOURCE_DIR}/sdk/include/glad/gl.h"
    )
endif()

if (NOT CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" AND NOT CMAKE_SYSTEM_NAME STREQUAL "NetBSD" AND NOT WIN32 AND NOT HAIKU)
    target_link_libraries(xrImGui
        PRIVATE
        $<$<NOT:$<PLATFORM_ID:OpenBSD>>:dl>
    )
endif()

target_compile_definitions(xrImGui
    PUBLIC
    IMGUI_DISABLE_OBSOLETE_KEYIO
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DEFINE_MATH_OPERATORS

    PRIVATE
    IMGUI_EXPORTS
)

set_target_properties(xrImGui PROPERTIES
    PREFIX ""
    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)
