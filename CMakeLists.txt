cmake_minimum_required(VERSION 3.5)
project(SimpleChat)

find_package(Boost REQUIRED)

find_package(Threads)






add_executable(Client ChatClient.cpp ChatNetwork.hpp)


set_target_properties(
    Client PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(
    Client
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    )

add_executable(Server ChatServer.cpp ChatNetwork.hpp)

set_target_properties(
    Server PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(
    Server
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
