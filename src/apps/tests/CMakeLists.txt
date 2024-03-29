project(tests VERSION "${CMAKE_PROJECT_VERSION}")

set(BINANCE_TESTNET_APIKEY "" CACHE STRING "Binance API Key")
set(BINANCE_TESTNET_SECRETKEY "" CACHE STRING "Binance Secret Key")

configure_file(testnet_secrets.h.in testnet_secrets.h @ONLY)

set(${PROJECT_NAME}_HEADERS
    catch.hpp
    Utilities.h
    ${CMAKE_CURRENT_BINARY_DIR}/testnet_secrets.h
)

set(${PROJECT_NAME}_SOURCES
    main.cpp

    Binance_tests.cpp
    Cryptography_tests.cpp
    Database_tests.cpp
    Decimal_tests.cpp
    Exchange_tests.cpp
    Order_tests.cpp
    Spawn_tests.cpp
    Spreaders_tests.cpp
    StableDownSpread_tests.cpp
    Sqliteorm_tests.cpp
    Trader_tests.cpp
    Websocket_tests.cpp
)

add_executable(${PROJECT_NAME}
               ${${PROJECT_NAME}_HEADERS}
               ${${PROJECT_NAME}_SOURCES}
)

# To make the generate secrets file accessible
target_include_directories(${PROJECT_NAME} PRIVATE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

find_package(spdlog REQUIRED COMPONENTS spdlog)
find_package(SqliteOrm REQUIRED)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ad::binance
        ad::tradebot
        ad::websocket

        spdlog::spdlog
        sqlite_orm::sqlite_orm
)

set_target_properties(${PROJECT_NAME} PROPERTIES
                      VERSION "${${PROJECT_NAME}_VERSION}"
)

install(TARGETS ${PROJECT_NAME})
