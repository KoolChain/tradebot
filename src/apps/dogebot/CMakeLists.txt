project(dogebot VERSION "${CMAKE_PROJECT_VERSION}")

set(${PROJECT_NAME}_HEADERS
    NaiveBot.h
    ProductionBot.h
)

set(${PROJECT_NAME}_SOURCES
    main.cpp
    ProductionBot.cpp
)

add_executable(${PROJECT_NAME}
               ${${PROJECT_NAME}_HEADERS}
               ${${PROJECT_NAME}_SOURCES}
)

find_package(spdlog REQUIRED COMPONENTS libspdlog)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ad::binance
        ad::tradebot

        spdlog::libspdlog
)

set_target_properties(${PROJECT_NAME} PROPERTIES
                      VERSION "${${PROJECT_NAME}_VERSION}"
)

install(TARGETS ${PROJECT_NAME})
