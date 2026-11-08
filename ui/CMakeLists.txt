cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" NAMAZ_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${NAMAZ_VERSION_STRING}" NAMAZ_VERSION_STRING)
project(namaz-ui VERSION ${NAMAZ_VERSION_STRING} LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport DBus Network LinguistTools)

# Rust state library (config, prayer times, Hijri, Qibla, calendars).
find_program(CARGO_EXECUTABLE cargo REQUIRED)
set(CORE_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo")
if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
    set(CARGO_PROFILE "release")
    set(CARGO_PROFILE_FLAG "--release")
else()
    set(CARGO_PROFILE "debug")
    set(CARGO_PROFILE_FLAG "")
endif()
set(CORE_LIB
    "${CORE_TARGET_DIR}/${CARGO_PROFILE}/${CMAKE_SHARED_LIBRARY_PREFIX}namaz_core${CMAKE_SHARED_LIBRARY_SUFFIX}")
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../crates/core/src/*.rs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../crates/prayer-core/src/*.rs")
add_custom_command(
    OUTPUT "${CORE_LIB}"
    COMMAND "${CARGO_EXECUTABLE}" build -p namaz-core ${CARGO_PROFILE_FLAG}
            --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/../Cargo.toml"
            --target-dir "${CORE_TARGET_DIR}"
    DEPENDS
        ${CORE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../Cargo.toml"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Cargo.lock"
        "${CMAKE_CURRENT_SOURCE_DIR}/../crates/core/Cargo.toml"
        "${CMAKE_CURRENT_SOURCE_DIR}/../crates/prayer-core/Cargo.toml"
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets/cities.json"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    COMMENT "Building Rust namaz-core (${CARGO_PROFILE})"
    VERBATIM
)
add_custom_target(namaz_core ALL DEPENDS "${CORE_LIB}")

add_executable(namaz-ui
    src/main.cpp
    src/Widgets.cpp
    src/Widgets.h
    src/Views.cpp
    src/Views.h
    src/UiUtils.cpp
    src/UiUtils.h
    src/DBusService.cpp
    src/DBusService.h
    src/Core.cpp
    src/Core.h
    src/Dialogs.cpp
    src/Dialogs.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
)

add_dependencies(namaz-ui namaz_core)

# Build the .qm file explicitly.  qt_add_translations() defers a call with
# unquoted arguments and breaks when the build directory contains spaces
# (for example ".../Моя Информация/...").  A plain lrelease command is
# smaller and works with any path.
set(NAMAZ_TS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/translations/namaz_ru.ts")
set(NAMAZ_QM_FILE "${CMAKE_CURRENT_BINARY_DIR}/namaz_ru.qm")
set_source_files_properties("${NAMAZ_QM_FILE}" PROPERTIES GENERATED TRUE)
add_custom_command(
    OUTPUT "${NAMAZ_QM_FILE}"
    COMMAND $<TARGET_FILE:Qt6::lrelease> "${NAMAZ_TS_FILE}" -qm "${NAMAZ_QM_FILE}"
    DEPENDS "${NAMAZ_TS_FILE}"
    COMMENT "Generating namaz_ru.qm"
    VERBATIM
)
add_custom_target(namaz_translations ALL DEPENDS "${NAMAZ_QM_FILE}")
add_dependencies(namaz-ui namaz_translations)
qt_add_resources(namaz-ui "namaz_translations"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES "${NAMAZ_QM_FILE}"
)

add_custom_target(update_translations
    COMMAND $<TARGET_FILE:Qt6::lupdate> -no-obsolete
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            -ts "${NAMAZ_TS_FILE}"
    COMMENT "Updating namaz_ru.ts"
    VERBATIM
)

qt_add_resources(namaz-ui "app_assets"
    PREFIX "/assets"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/../assets/skins/app-icon.png"
)

target_include_directories(namaz-ui PRIVATE src)
target_compile_definitions(namaz-ui PRIVATE
    NAMAZ_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(namaz-ui PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::DBus Qt6::Network "${CORE_LIB}")
set_target_properties(namaz-ui PROPERTIES
    OUTPUT_NAME namaz
    BUILD_RPATH "${CORE_TARGET_DIR}/${CARGO_PROFILE};$ORIGIN/../lib"
    INSTALL_RPATH "$ORIGIN/../lib"
)

install(TARGETS namaz-ui RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(namaz-tests
        tests/tst_dialogs.cpp
        src/Dialogs.cpp
        src/Dialogs.h
        src/Views.cpp
        src/Views.h
        src/Widgets.cpp
        src/Widgets.h
        src/DBusService.cpp
        src/DBusService.h
        src/UiUtils.cpp
        src/UiUtils.h
        src/Core.cpp
        src/Core.h
        src/UpdateChecker.cpp
        src/UpdateChecker.h
    )
    add_dependencies(namaz-tests namaz_core)
    qt_add_resources(namaz-tests "test_app_assets"
        PREFIX "/assets"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/../assets/skins/app-icon.png"
    )
    target_include_directories(namaz-tests PRIVATE src)
    target_link_libraries(namaz-tests PRIVATE
        Qt6::Widgets Qt6::PrintSupport Qt6::DBus Qt6::Network Qt6::Test
        "${CORE_LIB}")
    set_target_properties(namaz-tests PROPERTIES
        BUILD_RPATH "${CORE_TARGET_DIR}/${CARGO_PROFILE};$ORIGIN/../lib")
    add_test(NAME namaz-tests COMMAND namaz-tests)
    set_tests_properties(namaz-tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
