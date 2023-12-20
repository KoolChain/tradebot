project(websocket VERSION "${CMAKE_PROJECT_VERSION}")

set(${PROJECT_NAME}_HEADERS
    WebSocket.h
)

set(${PROJECT_NAME}_SOURCES
    WebSocket.cpp
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
        jsonformoderncpp::jsonformoderncpp
    PRIVATE
        Boost::boost
        spdlog::spdlog
        OpenSSL::SSL
)

set_target_properties(${PROJECT_NAME} PROPERTIES
                      VERSION "${${PROJECT_NAME}_VERSION}"
)

if(MSVC)
    # Otherwise big object files might error with:
    # "fatal  error C1128: number of sections exceeded object file format limit: compile with /bigobj"
    target_compile_options(${PROJECT_NAME} PRIVATE "/bigobj")
endif()


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
