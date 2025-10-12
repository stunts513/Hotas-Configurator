Download Dear ImGui from https://github.com/ocornut/imgui and place the following files in external/imgui:
- imgui.h, imgui.cpp
- imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp
- backends/imgui_impl_win32.h, imgui_impl_win32.cpp
- backends/imgui_impl_dx9.h, imgui_impl_dx9.cpp

This project uses Win32 and DirectX9 for the GUI backend. You can change the backend if needed.

After placing the files, CMake will build Dear ImGui with your project.