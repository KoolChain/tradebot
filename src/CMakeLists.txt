add_subdirectory(libs/binance/binance)
add_subdirectory(libs/storage/storage)
add_subdirectory(libs/websocket/websocket)

add_subdirectory(libs/tradebot/tradebot)

add_subdirectory(apps/dogebot)
option (BUILD_tests "Build 'tests' application" true)
if(BUILD_tests)
    add_subdirectory(apps/tests)
endif()
