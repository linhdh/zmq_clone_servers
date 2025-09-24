cmake_minimum_required(VERSION 4.0)
project(zmq_clone_servers)

set(CMAKE_CXX_STANDARD 17)

add_executable(clonecli1
        clonepattern1/clonecli1.cpp
        clonepattern1/clonecli1.h
        commons/kvsimple.hpp
        commons/kvsimple.cpp
        commons/zhelpers.hpp
        clonepattern2/clonesrv2.cpp
        clonepattern2/clonesrv2.h)
target_link_libraries(clonecli1 zmq)

add_executable(clonesrv1
        clonepattern1/clonesrv1.cpp
        clonepattern1/clonesrv1.h
        commons/kvsimple.hpp
        commons/kvsimple.cpp)
target_link_libraries(clonesrv1 zmq)

add_executable(clonesrv2
        clonepattern2/clonesrv2.cpp
        clonepattern2/clonesrv2.h
        commons/kvsimple.hpp
        commons/kvsimple.cpp)
target_link_libraries(clonesrv2 zmq)

add_executable(clonecli2
        clonepattern2/clonecli2.cpp
        clonepattern2/clonecli2.h
        commons/kvsimple.hpp
        commons/kvsimple.cpp)
target_link_libraries(clonecli2 zmq)

add_executable(clonesrv3 clonepattern3/clonesrv3.cpp
        clonepattern3/clonesrv3.h
        commons/kvsimple.hpp
        commons/kvsimple.cpp)
target_link_libraries(clonesrv3 zmq)

add_executable(clonecli3
        clonepattern3/clonecli3.cpp
        clonepattern3/clonecli3.h
        commons/kvsimple.hpp
        commons/kvsimple.cpp)
target_link_libraries(clonecli3 zmq)

add_executable(clonecli4
        clonepattern4/clonecli4.cpp
        clonepattern4/clonecli4.h
        commons/kvsimple.hpp
        commons/kvsimple.cpp)
target_link_libraries(clonecli4 zmq)

add_executable(clonesrv4
        clonepattern4/clonesrv4.cpp
        clonepattern4/clonesrv4.h
        commons/kvsimple.hpp
        commons/kvsimple.cpp)
target_link_libraries(clonesrv4 zmq)

add_executable(clonesrv5
        commons/kvmsg.cpp
        commons/kvmsg.hpp
        clonepattern5/clonesrv5.cpp
        clonepattern5/clonesrv5.h)
target_link_libraries(clonesrv5 zmq zmqpp)

add_executable(clonecli5
        clonepattern5/clonecli5.cpp
        clonepattern5/clonecli5.h
        commons/kvmsg.hpp
        commons/kvmsg.cpp)
target_link_libraries(clonecli5 zmq zmqpp)
