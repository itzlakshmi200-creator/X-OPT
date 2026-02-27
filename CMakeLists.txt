cmake_minimum_required(VERSION 3.20)
project(X-OPT VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Find Dear ImGui via vcpkg ────────────────────────────────────────────────
find_package(imgui CONFIG REQUIRED)

# ─── Win32 manifest for UAC elevation request ─────────────────────────────────
set(APP_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/src/app.manifest")

# ─── Executable (GUI, no console window) ─────────────────────────────────────
add_executable(XOPT WIN32
    src/main.cpp
)

# Embed manifest if it exists
if(EXISTS "${APP_MANIFEST}")
    target_sources(XOPT PRIVATE "${APP_MANIFEST}")
endif()

# ─── Link dependencies ────────────────────────────────────────────────────────
target_link_libraries(XOPT PRIVATE
    imgui::imgui
    d3d11
    dxgi
    dwmapi
    powrprof
    winmm
    psapi
    shell32
    comdlg32
    user32
    gdi32
)

# ─── Compiler flags ──────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(XOPT PRIVATE
        /W3
        /O2           # Full optimisation
        /fp:fast      # Fast floating point
        /arch:SSE2
        /MP           # Multi-processor compilation
        /wd4244       # Suppress narrowing warnings (ImGui internals)
        /wd4267
    )
    target_compile_definitions(XOPT PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )
    # Link-time code generation
    set_property(TARGET XOPT PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# ─── Output naming ────────────────────────────────────────────────────────────
set_target_properties(XOPT PROPERTIES
    OUTPUT_NAME "X-OPT"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/debug"
)

# ─── Install ─────────────────────────────────────────────────────────────────
install(TARGETS XOPT DESTINATION bin)
