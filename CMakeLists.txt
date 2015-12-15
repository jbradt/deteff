find_package(HDF5 COMPONENTS CXX REQUIRED)
include_directories(SYSTEM ${HDF5_INCLUDE_DIRS})

find_package(YamlCpp REQUIRED)
include_directories(SYSTEM ${YAMLCPP_INCLUDE_DIR})

find_package(SQLite3 REQUIRED)
include_directories(SYSTEM ${SQLITE3_INCLUDE_DIRS})

find_package(pugixml REQUIRED)
include_directories(SYSTEM ${pugixml_INCLUDE_DIRS})

add_executable(deteff deteff.cpp)
target_link_libraries(deteff mcopt pugixml ${HDF5_CXX_LIBRARIES} ${ARMADILLO_LIBRARIES} ${YAMLCPP_LIBRARY} ${SQLITE3_LIBRARIES})
