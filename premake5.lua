workspace "StimLink"
    architecture "x86_64"
    configurations { "Debug", "Release" }
    startproject "StimLink"

outputdir = "%{cfg.buildcfg}/%{cfg.architecture}"

project "StimLink"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-obj/" .. outputdir .. "/%{prj.name}")

    files
    {
        -- StimLink
        "src/**.h",
        "src/**.cpp",

        -- ImGui
        "external/ImGui/imgui.cpp",
        "external/ImGui/imgui_draw.cpp",
        "external/ImGui/imgui_tables.cpp",
        "external/ImGui/imgui_widgets.cpp",

        "external/ImGui/backends/imgui_impl_win32.cpp",
        "external/ImGui/backends/imgui_impl_dx11.cpp"
    }

    includedirs
    {
        "src",

        "external/libVIIPER/include",

        "external/imgui",
        "external/imgui/backends"
    }

    libdirs
    {
        "external/libVIIPER/lib"
    }

    links
    {
        "windowsapp",

        -- VIIPER
        "libVIIPER",

        -- ImGui
        "d3d11",
        "dxgi",
        "d3dcompiler"
    }

    postbuildcommands
    {
        -- Copy libVIIPER.dll
        '{COPYFILE} "%{wks.location}/external/libVIIPER/bin/libVIIPER.dll" "%{cfg.targetdir}/libVIIPER.dll"',

        '{MKDIR} "%{cfg.targetdir}/external/USBip"',
        
        -- Copy usbip.exe
        '{COPYFILE} "external/USBip/usbip.exe" "%{cfg.targetdir}/external/USBip/usbip.exe"',

        -- Copy libusbip.dll
        '{COPYFILE} "external/USBip/libusbip.dll" "%{cfg.targetdir}/external/usbip/libusbip.dll"',

        -- Copy resources.dll
        '{COPYFILE} "external/USBip/resources.dll" "%{cfg.targetdir}/external/USBip/resources.dll"'
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines { "TC_DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter {}