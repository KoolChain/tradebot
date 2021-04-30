add_subdirectory(libs/tradebot/tradebot)

option (BUILD_tests "Build 'tests' application" true)
if(BUILD_tests)
    add_subdirectory(apps/tests)
endif()
