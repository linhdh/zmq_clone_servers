cmake_minimum_required(VERSION 4.0)
project(zmq_clone_servers)

set(CMAKE_CXX_STANDARD 17)

add_executable(clonecli1
        clonepattern1/clonecli1.cpp
        clonepattern1/clonecli1.h
        clonepattern1/kvsimple.hpp
        clonepattern1/kvsimple.cpp
        clonepattern1/zhelpers.hpp)
target_link_libraries(clonecli1 zmq)

add_executable(clonesrv1
        clonepattern1/clonesrv1.cpp
        clonepattern1/clonesrv1.h
        clonepattern1/kvsimple.hpp
        clonepattern1/kvsimple.cpp)
target_link_libraries(clonesrv1 zmq)