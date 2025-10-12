cmake_minimum_required(VERSION 3.10)

project(HotasConfigurator)

set(CMAKE_CXX_STANDARD 11)

# ImGui sources (assumes you placed files as described in external/imgui-readme.txt)

file(GLOB IMGUI_SRC
    "external/imgui/imgui*.cpp"
    "external/imgui/backends/imgui_impl_win32.cpp"
    "external/imgui/backends/imgui_impl_dx11.cpp"
)


add_executable(HotasConfigurator src/main.cpp ${IMGUI_SRC})

# Add ImGui include directory
target_include_directories(HotasConfigurator PRIVATE external/imgui external/imgui/backends)

# Link DirectInput, DXGUID, DirectX9, and Windows libraries
if(MSVC)
    target_link_libraries(HotasConfigurator dinput8 dxguid d3d11)
endif()
target_link_libraries(HotasConfigurator user32)
