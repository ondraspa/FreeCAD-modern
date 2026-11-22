# SPDX-License-Identifier: LGPL-2.1-or-later

disable_occt8_deprecation_warnings()

add_subdirectory(App)
if(BUILD_GUI)
    add_subdirectory(Gui)
endif()

set(Drawing_Scripts
    Init.py
)

if(BUILD_GUI)
    list(APPEND Drawing_Scripts InitGui.py)
endif()

add_custom_target(Drawing_Data ALL
    SOURCES ${Drawing_Scripts}
)

fc_target_copy_resource(Drawing_Data
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/Mod/Drawing
    ${Drawing_Scripts}
)

install(
    FILES ${Drawing_Scripts}
    DESTINATION Mod/Drawing
)
