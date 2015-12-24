find_package(HDF5 COMPONENTS CXX REQUIRED)
include_directories(SYSTEM ${HDF5_INCLUDE_DIRS})

find_package(YamlCpp REQUIRED)
include_directories(SYSTEM ${YAMLCPP_INCLUDE_DIR})

find_package(SQLite3 REQUIRED)
include_directories(SYSTEM ${SQLITE3_INCLUDE_DIRS})

find_package(pugixml REQUIRED)
include_directories(SYSTEM ${pugixml_INCLUDE_DIRS})

set(DETEFF_SOURCES PadMap.cpp parsers.cpp SQLiteWrapper.cpp)
set(DETEFF_REQUIRED_LIBRARIES mcopt ${pugixml_LIBRARIES} ${HDF5_CXX_LIBRARIES} ${ARMADILLO_LIBRARIES} ${YAMLCPP_LIBRARY} ${SQLITE3_LIBRARIES})

add_executable(deteff deteff.cpp ${DETEFF_SOURCES})
target_link_libraries(deteff pugixml ${DETEFF_REQUIRED_LIBRARIES})
target_compile_features(deteff PUBLIC cxx_return_type_deduction)

add_executable(test_deteff catch_main.cpp test_sqlwrapper.cpp ${DETEFF_SOURCES})
target_link_libraries(test_deteff pugixml ${DETEFF_REQUIRED_LIBRARIES})
