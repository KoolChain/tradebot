add_subdirectory(libs/trademath/trademath)
add_subdirectory(libs/binance/binance)
add_subdirectory(libs/websocket/websocket)

add_subdirectory(libs/tradebot/tradebot)

add_subdirectory(apps/dogebot)
add_subdirectory(apps/binance-cli)
add_subdirectory(apps/initial-fragments)
add_subdirectory(apps/tradelist)
option (BUILD_tests "Build 'tests' application" true)
if(BUILD_tests)
    add_subdirectory(apps/tests)
endif()
