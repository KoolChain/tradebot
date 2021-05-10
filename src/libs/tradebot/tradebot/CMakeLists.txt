project(tradebot VERSION "${CMAKE_PROJECT_VERSION}")

set(${PROJECT_NAME}_HEADERS
    Database.h
    Fragment.h
    Order.h
    OrmAdaptors-impl.h
)

set(${PROJECT_NAME}_SOURCES
    Database.cpp
    Order.cpp
    Fragment.cpp
    Trader.cpp
)

cmc_find_dependencies()

add_library(${PROJECT_NAME}
            ${${PROJECT_NAME}_SOURCES}
            ${${PROJECT_NAME}_HEADERS}
)

add_library(ad::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

cmc_target_current_include_directory(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
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
