workspace "CoyoteController"
    architecture "x86_64"
    configurations { "Debug", "Release" }
    startproject "CoyoteController"

outputdir = "%{cfg.buildcfg}/%{cfg.architecture}"

project "CoyoteController"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-obj/" .. outputdir .. "/%{prj.name}")

    files
    {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "external/libVIIPER/include"
    }

    libdirs
    {
        "external/libVIIPER/lib"
    }

    links
    {
        "libVIIPER"
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
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter {}