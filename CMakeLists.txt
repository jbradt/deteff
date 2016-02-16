cmake_minimum_required(VERSION 3.1)
project(deteff VERSION 2.1.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Set up RPATH for build products
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
#set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_CXX_STANDARD 14)

if(APPLE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-2")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -gdwarf-2")
endif()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DARMA_NO_DEBUG")

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -Wno-c++98-compat -Wno-padded -Wno-weak-vtables -Wno-shadow -Wno-exit-time-destructors")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra -Wfloat-equal -Wpointer-arith -Wcast-qual")
endif()

find_package(OpenMP)
if("${OPENMP_FOUND}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
else()
    message(WARNING "OpenMP not available. Output will be single-threaded.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-source-uses-openmp")
endif()

find_package(HDF5 COMPONENTS CXX REQUIRED)
include_directories(SYSTEM ${HDF5_INCLUDE_DIRS})

find_package(YamlCpp REQUIRED)
include_directories(SYSTEM ${YAMLCPP_INCLUDE_DIR})

find_package(Boost REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

find_package(SQLite3 REQUIRED)
include_directories(SYSTEM ${SQLITE3_INCLUDE_DIRS})

find_package(pugixml REQUIRED)
include_directories(SYSTEM ${pugixml_INCLUDE_DIRS})

find_package(mcopt REQUIRED)
include_directories(${mcopt_INCLUDE_DIRS})

set(DETEFF_SOURCES src/deteff.cpp src/parsers.cpp src/SQLiteWrapper.cpp)
include_directories(include)

add_executable(deteff ${DETEFF_SOURCES})
target_link_libraries(deteff mcopt pugixml ${HDF5_CXX_LIBRARIES} ${ARMADILLO_LIBRARIES} ${YAMLCPP_LIBRARY} ${SQLITE3_LIBRARIES})
install(TARGETS deteff RUNTIME DESTINATION bin)

set(TEST_DETEFF_SOURCES test/catch_main.cpp test/test_sqlwrapper.cpp src/parsers.cpp src/SQLiteWrapper.cpp)

add_executable(test_deteff ${TEST_DETEFF_SOURCES})
target_link_libraries(test_deteff mcopt pugixml ${HDF5_CXX_LIBRARIES} ${ARMADILLO_LIBRARIES} ${YAMLCPP_LIBRARY} ${SQLITE3_LIBRARIES})
