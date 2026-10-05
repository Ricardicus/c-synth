cmake_minimum_required(VERSION 3.16)
project(libcsynth VERSION 0.1.0 LANGUAGES C)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(CSYNTH_BUILD_TESTS "Build the libcsynth tests" ${BUILD_TESTING})
option(CSYNTH_BUILD_EXAMPLES "Build the WAV rendering example" OFF)

function(csynth_compile_options target)
    set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(csynth STATIC src/synth.c src/effects.c src/envelope.c src/synth_config.c
                          src/oscillator.c src/fm.c src/presets.c)
add_library(csynth::csynth ALIAS csynth)
target_compile_features(csynth PUBLIC c_std_11)
target_include_directories(csynth PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/csynth>)
if(UNIX)
    target_link_libraries(csynth PUBLIC m)
endif()
set_target_properties(csynth PROPERTIES POSITION_INDEPENDENT_CODE ON)
csynth_compile_options(csynth)

if(CSYNTH_BUILD_EXAMPLES)
    add_executable(csynth_live examples/live.c)
    target_link_libraries(csynth_live PRIVATE csynth::csynth)
    csynth_compile_options(csynth_live)
endif()

install(TARGETS csynth EXPORT csynthTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/synth.h include/synth_config.h include/fm.h include/oscillator.h
              include/envelope.h include/effects.h include/presets.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/csynth)
install(EXPORT csynthTargets NAMESPACE csynth:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/csynth)
configure_package_config_file(cmake/csynthConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/csynthConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/csynth)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/csynthConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/csynthConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/csynthConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/csynth)
install(FILES README.md docs/libcsynth-api.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY presets/factory DESTINATION ${CMAKE_INSTALL_DATADIR}/csynth/presets)

if(CSYNTH_BUILD_TESTS)
    enable_testing()
    foreach(test_name IN ITEMS presets factory_bank effects envelope fm engine)
        add_executable(test_${test_name} tests/test_${test_name}.c)
        target_link_libraries(test_${test_name} PRIVATE csynth::csynth)
        csynth_compile_options(test_${test_name})
        add_test(NAME ${test_name} COMMAND test_${test_name})
    endforeach()
    target_compile_definitions(test_factory_bank PRIVATE FACTORY_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/presets")
endif()
