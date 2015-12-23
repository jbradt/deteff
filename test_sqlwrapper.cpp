#include "catch.hpp"
#include "SQLiteWrapper.h"
#include <armadillo>
#include <vector>
#include <string>
#include <iostream>

TEST_CASE("SQLite3 interface works", "[sqlite3]")
{
    sqlite::SQLiteDatabase writer (":memory:");

    arma::mat data = {{1, 2, 3},
                      {4, 5, 6},
                      {7, 8, 9},
                      {10, 11, 12}};

    std::string tableName = "test";
    std::vector<sqlite::SQLColumn> tableSpec {sqlite::SQLColumn("a", "REAL"),
                                              sqlite::SQLColumn("b", "REAL"),
                                              sqlite::SQLColumn("c", "REAL")};

    SECTION("Round-trip to database is identity")
    {
        writer.createTable(tableName, tableSpec);
        writer.insertIntoTable(tableName, data);

        arma::mat result = writer.readTable(tableName);

        REQUIRE( result.n_cols == data.n_cols );
        REQUIRE( result.n_rows == data.n_rows );

        REQUIRE( arma::accu(arma::abs(data - result)) < 1e-6 );
    }
}
