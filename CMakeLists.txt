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

set(DETEFF_SOURCES parsers.cpp SQLiteWrapper.cpp)
set(DETEFF_REQUIRED_LIBRARIES mcopt pugixml ${HDF5_CXX_LIBRARIES} ${ARMADILLO_LIBRARIES} ${YAMLCPP_LIBRARY} ${SQLITE3_LIBRARIES})

add_executable(deteff deteff.cpp ${DETEFF_SOURCES})
target_link_libraries(deteff ${DETEFF_REQUIRED_LIBRARIES})
install(TARGETS deteff RUNTIME DESTINATION bin)

add_executable(test_deteff catch_main.cpp test_sqlwrapper.cpp ${DETEFF_SOURCES})
target_link_libraries(test_deteff ${DETEFF_REQUIRED_LIBRARIES})
