project(tradebot VERSION "${CMAKE_PROJECT_VERSION}")

set(${PROJECT_NAME}_HEADERS
    Database.h
    Exchange.h
    Fragment.h
    Fulfillment.h
    Logging.h
    Order.h
    OrmAdaptors-impl.h
    OrmDecimalAdaptor-impl.h
    Spawner.h
    Stream.h
    SymbolFilters.h
    Trader.h

    spawners/Helpers.h
    spawners/NaiveDownSpread.h
    spawners/StableDownSpread.h
)

set(${PROJECT_NAME}_SOURCES
    Database.cpp
    Exchange.cpp
    Order.cpp
    Fragment.cpp
    Fulfillment.cpp
    Stream.cpp
    Trader.cpp

    spawners/NaiveDownSpread.cpp
)

cmc_find_dependencies()

add_library(${PROJECT_NAME}
            ${${PROJECT_NAME}_SOURCES}
            ${${PROJECT_NAME}_HEADERS}
)

add_library(ad::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

cmc_target_current_include_directory(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ad::binance
        ad::websocket

        Boost::boost
    PRIVATE
        spdlog::libspdlog
        sqlite_orm::sqlite_orm
)

set_target_properties(${PROJECT_NAME} PROPERTIES
                      VERSION "${${PROJECT_NAME}_VERSION}"
)


##
## Install
##

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Targets)

include(cmc-install)
# Install the headers preserving the directory hierarchy
cmc_install_with_folders(FILES ${${PROJECT_NAME}_HEADERS}
                         DESTINATION include/${PROJECT_NAME}/${PROJECT_NAME}
)
cmc_install_packageconfig(${PROJECT_NAME} ${PROJECT_NAME}Targets
                          FIND_FILE "CMakeFinds.cmake.in"
                          NAMESPACE ad::
)
