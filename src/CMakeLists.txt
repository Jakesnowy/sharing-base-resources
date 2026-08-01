project(ModIntegratedStorageCpp)

set(TARGET ModIntegratedStorageCpp)

set(${TARGET}_Sources
        "${CMAKE_CURRENT_SOURCE_DIR}/src/dllmain.cpp"
        )

add_library(${TARGET} SHARED ${${TARGET}_Sources})
target_link_libraries(${TARGET} PRIVATE PolyHook_2)
target_link_libraries(${TARGET} PUBLIC UE4SS)
