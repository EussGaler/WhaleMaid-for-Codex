cmake_minimum_required(VERSION 3.24)

project(WhaleMaidPetV3 VERSION 1.3.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL Network Test)
qt_standard_project_setup()

set(WHALE_LIVE2D_SDK_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/CubismSdkForNative-5-r.5"
    CACHE PATH
    "Path to Live2D Cubism SDK for Native")

option(WHALE_ENABLE_LIVE2D "Build the Cubism Native OpenGL renderer" ON)

qt_add_executable(WhaleMaidPet
    src/main.cpp
    src/PetStateController.cpp
    src/PetStateController.hpp
    src/PetPreferences.cpp
    src/PetPreferences.hpp
    src/PlatformPaths.cpp
    src/PlatformPaths.hpp
    src/WindowPlacement.cpp
    src/WindowPlacement.hpp
    src/CharacterWidget.cpp
    src/CharacterWidget.hpp
    src/CodexStatusBridge.cpp
    src/CodexStatusBridge.hpp
    src/CodexActivityWatcher.cpp
    src/CodexActivityWatcher.hpp
    src/StartupSettings.cpp
    src/StartupSettings.hpp
    src/PetWindow.cpp
    src/PetWindow.hpp
)

if(WIN32)
    qt_add_executable(WhaleMaidHook
        src/hook_main.cpp
        src/CodexStatusBridge.cpp
        src/CodexStatusBridge.hpp
    )
endif()

target_link_libraries(WhaleMaidPet PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Network
)
if(TARGET WhaleMaidHook)
    target_link_libraries(WhaleMaidHook PRIVATE
        Qt6::Core
        Qt6::Network
    )
    target_compile_definitions(WhaleMaidHook PRIVATE QT_NO_KEYWORDS)
endif()
target_compile_definitions(WhaleMaidPet PRIVATE QT_NO_KEYWORDS)

if(WHALE_ENABLE_LIVE2D)
    if(NOT EXISTS "${WHALE_LIVE2D_SDK_ROOT}/Core/include/Live2DCubismCore.h")
        message(FATAL_ERROR "Live2D Cubism SDK not found: ${WHALE_LIVE2D_SDK_ROOT}")
    endif()

    set(CUBISM_CORE_PATH "${WHALE_LIVE2D_SDK_ROOT}/Core")
    set(CUBISM_FRAMEWORK_PATH "${WHALE_LIVE2D_SDK_ROOT}/Framework")
    set(CUBISM_GLEW_PATH "${WHALE_LIVE2D_SDK_ROOT}/Samples/OpenGL/thirdParty/glew")

    set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
    set(ONLY_LIBS ON CACHE BOOL "" FORCE)
    add_subdirectory(
        "${CUBISM_GLEW_PATH}/build/cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/glew"
        EXCLUDE_FROM_ALL)

    add_library(Live2DCubismCore STATIC IMPORTED GLOBAL)
    if(WIN32)
        set(CUBISM_CORE_LIBRARY_DIR "${CUBISM_CORE_PATH}/lib/windows/x86_64/143")
        set_target_properties(Live2DCubismCore PROPERTIES
            IMPORTED_LOCATION_RELEASE "${CUBISM_CORE_LIBRARY_DIR}/Live2DCubismCore_MD.lib"
            IMPORTED_LOCATION_DEBUG "${CUBISM_CORE_LIBRARY_DIR}/Live2DCubismCore_MDd.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${CUBISM_CORE_PATH}/include")
        set(CUBISM_PLATFORM_DEFINITION CSM_TARGET_WIN_GL)
    elseif(UNIX AND NOT APPLE)
        set_target_properties(Live2DCubismCore PROPERTIES
            IMPORTED_LOCATION "${CUBISM_CORE_PATH}/lib/linux/x86_64/libLive2DCubismCore.a"
            INTERFACE_INCLUDE_DIRECTORIES "${CUBISM_CORE_PATH}/include")
        set(CUBISM_PLATFORM_DEFINITION CSM_TARGET_LINUX_GL)
    else()
        message(FATAL_ERROR "Live2D renderer is currently configured for Windows and Linux")
    endif()

    set(FRAMEWORK_SOURCE OpenGL)
    set(RENDER_INCLUDE_PATH "${CUBISM_GLEW_PATH}/include")
    add_subdirectory(
        "${CUBISM_FRAMEWORK_PATH}"
        "${CMAKE_CURRENT_BINARY_DIR}/CubismFramework")
    target_compile_definitions(Framework PUBLIC
        ${CUBISM_PLATFORM_DEFINITION}
        GLEW_STATIC)
    target_include_directories(Framework PUBLIC "${CUBISM_GLEW_PATH}/include")
    target_link_libraries(Framework PUBLIC Live2DCubismCore glew_s)

    find_package(OpenGL REQUIRED)

    target_sources(WhaleMaidPet PRIVATE
        src/live2d/CubismAllocator.cpp
        src/live2d/CubismAllocator.hpp
        src/live2d/CubismRuntime.cpp
        src/live2d/CubismRuntime.hpp
        src/live2d/PetCubismModel.cpp
        src/live2d/PetCubismModel.hpp
        src/live2d/Live2DScene.cpp
        src/live2d/Live2DScene.hpp
    )
    target_include_directories(WhaleMaidPet PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CUBISM_FRAMEWORK_PATH}/src"
        "${CUBISM_CORE_PATH}/include"
        "${CUBISM_GLEW_PATH}/include")
    target_link_libraries(WhaleMaidPet PRIVATE Framework glew_s OpenGL::GL)
    target_compile_definitions(WhaleMaidPet PRIVATE
        WHALE_HAS_LIVE2D=1
        ${CUBISM_PLATFORM_DEFINITION}
        GLEW_STATIC)

    if(UNIX AND NOT APPLE)
        qt_add_executable(WhaleMaidLive2DWindow
            src/live2d_window_main.cpp
            src/CharacterWidget.cpp
            src/CharacterWidget.hpp
            src/live2d/CubismAllocator.cpp
            src/live2d/CubismAllocator.hpp
            src/live2d/CubismRuntime.cpp
            src/live2d/CubismRuntime.hpp
            src/live2d/PetCubismModel.cpp
            src/live2d/PetCubismModel.hpp
            src/live2d/Live2DScene.cpp
            src/live2d/Live2DScene.hpp
        )
        target_include_directories(WhaleMaidLive2DWindow PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CUBISM_FRAMEWORK_PATH}/src"
            "${CUBISM_CORE_PATH}/include"
            "${CUBISM_GLEW_PATH}/include")
        target_link_libraries(WhaleMaidLive2DWindow PRIVATE
            Qt6::Widgets
            Qt6::OpenGLWidgets
            Qt6::OpenGL
            Framework
            glew_s
            OpenGL::GL)
        target_compile_definitions(WhaleMaidLive2DWindow PRIVATE
            QT_NO_KEYWORDS
            WHALE_HAS_LIVE2D=1
            ${CUBISM_PLATFORM_DEFINITION}
            GLEW_STATIC)
        target_compile_options(WhaleMaidLive2DWindow PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(MSVC)
    target_compile_options(WhaleMaidPet PRIVATE /utf-8 /W4 /permissive-)
    if(TARGET WhaleMaidHook)
        target_compile_options(WhaleMaidHook PRIVATE /utf-8 /W4 /permissive-)
    endif()
else()
    target_compile_options(WhaleMaidPet PRIVATE -Wall -Wextra -Wpedantic)
    if(TARGET WhaleMaidHook)
        target_compile_options(WhaleMaidHook PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

set_target_properties(WhaleMaidPet PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
if(UNIX AND NOT APPLE)
    set_target_properties(WhaleMaidPet PROPERTIES
        INSTALL_RPATH "$ORIGIN/lib")
endif()
if(TARGET WhaleMaidHook)
    set_target_properties(WhaleMaidHook PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET WhaleMaidPet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:WhaleMaidPet>/assets"
    )
endif()

if(WHALE_ENABLE_LIVE2D)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WhaleMaid")
        set(WHALE_LIVE2D_RUNTIME_TARGETS WhaleMaidPet)
        if(TARGET WhaleMaidLive2DWindow)
            list(APPEND WHALE_LIVE2D_RUNTIME_TARGETS WhaleMaidLive2DWindow)
        endif()
        foreach(WHALE_LIVE2D_RUNTIME_TARGET IN LISTS WHALE_LIVE2D_RUNTIME_TARGETS)
            add_custom_command(TARGET ${WHALE_LIVE2D_RUNTIME_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WhaleMaid"
                        "$<TARGET_FILE_DIR:${WHALE_LIVE2D_RUNTIME_TARGET}>/Resources/WhaleMaid"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard"
                        "$<TARGET_FILE_DIR:${WHALE_LIVE2D_RUNTIME_TARGET}>/FrameworkShaders"
            )
        endforeach()
    endif()
endif()

install(TARGETS WhaleMaidPet
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION .)
endif()
if(WHALE_ENABLE_LIVE2D AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WhaleMaid")
    install(DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WhaleMaid"
        DESTINATION Resources)
    install(DIRECTORY
        "${CUBISM_FRAMEWORK_PATH}/src/Rendering/OpenGL/Shaders/Standard/"
        DESTINATION FrameworkShaders)
endif()
if(TARGET WhaleMaidHook)
    install(TARGETS WhaleMaidHook
        RUNTIME DESTINATION .
    )
endif()

enable_testing()

qt_add_executable(WhaleMaidPetTests
    tests/PetStateControllerTests.cpp
    src/CodexActivityWatcher.cpp
    src/CodexActivityWatcher.hpp
    src/PetStateController.cpp
    src/PetStateController.hpp
    src/PetPreferences.cpp
    src/PetPreferences.hpp
    src/PlatformPaths.cpp
    src/PlatformPaths.hpp
    src/WindowPlacement.cpp
    src/WindowPlacement.hpp
    src/StartupSettings.cpp
    src/StartupSettings.hpp
)
target_link_libraries(WhaleMaidPetTests PRIVATE Qt6::Test)
target_compile_definitions(WhaleMaidPetTests PRIVATE QT_NO_KEYWORDS)
target_include_directories(WhaleMaidPetTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_test(NAME WhaleMaidPetTests COMMAND WhaleMaidPetTests)
