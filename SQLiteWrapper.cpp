#include "SQLiteWrapper.h"
#include <cassert>

namespace sqlite {

    SQLWriter::SQLWriter(const std::string& path)
    : path(path)
    {
        int status = 0;
        status = sqlite3_open(path.c_str(), &db);
        if (status != SQLITE_OK) {
            throw DBError(sqlite3_errmsg(db));
        }
    }

    SQLWriter::SQLWriter(const SQLWriter& rhs)
    : path(rhs.path)
    {
        int status = 0;
        status = sqlite3_open(path.c_str(), &db);
        if (status != SQLITE_OK) {
            throw DBError(sqlite3_errmsg(db));
        }
    }

    SQLWriter::~SQLWriter()
    {
        int status = 0;
        status = sqlite3_close(db);
    }

    void SQLWriter::createTable(const std::string& name, const std::vector<SQLColumn>& columns)
    {
        std::stringstream sqlss;
        sqlss << "CREATE TABLE " << name << " (idx INTEGER UNIQUE, ";
        for (int i = 0; i < columns.size(); i++) {
            sqlss << columns[i].getSQLrepr();
            if (i < columns.size() - 1) {
                sqlss << ", ";
            }
        }
        sqlss << ");";
        std::string sql = sqlss.str();

        int status = sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
    }

    void SQLWriter::insertIntoTable(const std::string& name, const arma::mat& data)
    {
        int status = 0;
        std::stringstream sqlss;
        sqlss << "INSERT INTO " << name << " VALUES (?, ";
        for (int i = 0; i < data.n_cols - 1; i++) {
            sqlss << "?, ";
        }
        sqlss << "?);";
        std::string sql = sqlss.str();

        sqlite3_stmt* stmt = NULL;

        try {
            status = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

            status = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
            if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

            for (arma::uword i = 0; i < data.n_rows; i++) {
                status = sqlite3_bind_int64(stmt, 1, i);
                if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

                for (arma::uword j = 0; j < data.n_cols; j++) {
                    status = sqlite3_bind_double(stmt, j+2, data(i, j));
                    if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
                }

                status = sqlite3_step(stmt);
                if (status != SQLITE_DONE) {
                    std::cerr << "Insertion failed: " << sqlite3_errmsg(db) << std::endl;
                }
                sqlite3_reset(stmt);
                sqlite3_clear_bindings(stmt);
            }
        }
        catch (const DBError&) {
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            stmt = NULL;
            throw;
        }

        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    arma::mat SQLWriter::readTable(const std::string& name)
    {
        int status = 0;
        std::string select_sql = "SELECT * FROM " + name + ";";
        std::string rowcnt_sql = "SELECT COUNT(*) FROM " + name + ";";
        sqlite3_stmt* select_stmt = NULL;
        sqlite3_stmt* rowcnt_stmt = NULL;
        arma::mat res;

        try {
            // Find the number of rows
            status = sqlite3_prepare_v2(db, rowcnt_sql.c_str(), -1, &rowcnt_stmt, NULL);
            if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

            status = sqlite3_step(rowcnt_stmt);
            if (status != SQLITE_ROW) throw DBError(sqlite3_errmsg(db));
            int64_t numRows = sqlite3_column_int64(rowcnt_stmt, 0);
            // std::cout << sqlite3_column_count(rowcnt_stmt) << std::endl;

            status = sqlite3_prepare_v2(db, select_sql.c_str(), -1, &select_stmt, NULL);
            if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

            int numCols = sqlite3_column_count(select_stmt);
            // std::cout << "Num rows = " << numRows << std::endl;
            // std::cout << "Num cols = " << numCols << std::endl;

            res.set_size(numRows, numCols-1);

            while (sqlite3_step(select_stmt) == SQLITE_ROW) {
                int64_t rowIdx = sqlite3_column_int64(select_stmt, 0);
                for (int j = 1; j < numCols; j++) {
                    // std::cout << "at (" << rowIdx << ", " << j << ")" << std::endl;
                    res(rowIdx, j-1) = sqlite3_column_double(select_stmt, j);
                }
            }
        }
        catch (const DBError&) {
            sqlite3_finalize(select_stmt);
            select_stmt = NULL;
            sqlite3_finalize(rowcnt_stmt);
            rowcnt_stmt = NULL;
            throw;
        }
        sqlite3_finalize(select_stmt);
        select_stmt = NULL;
        sqlite3_finalize(rowcnt_stmt);
        rowcnt_stmt = NULL;
        return res;
    }

}
