project(tests VERSION "${CMAKE_PROJECT_VERSION}")

set(BINANCE_TESTNET_APIKEY "" CACHE STRING "Binance API Key")
set(BINANCE_TESTNET_SECRETKEY "" CACHE STRING "Binance Secret Key")

configure_file(testnet_secrets.h.in testnet_secrets.h @ONLY)

set(${PROJECT_NAME}_HEADERS
    catch.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/testnet_secrets.h
)

set(${PROJECT_NAME}_SOURCES
    main.cpp

    Binance_tests.cpp
    Cryptography_tests.cpp
    Database_tests.cpp
    Tradebot_tests.cpp
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

find_package(spdlog REQUIRED COMPONENTS libspdlog)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ad::binance
        ad::tradebot
        ad::websocket

        spdlog::libspdlog
)

set_target_properties(${PROJECT_NAME} PROPERTIES
                      VERSION "${${PROJECT_NAME}_VERSION}"
)

install(TARGETS ${PROJECT_NAME})
