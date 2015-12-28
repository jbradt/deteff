#include "SQLiteWrapper.h"
#include <cassert>

namespace sqlite {

    SQLiteDatabase::SQLiteDatabase(const std::string& path)
    : path(path)
    {
        int status = 0;
        status = sqlite3_open(path.c_str(), &db);
        if (status != SQLITE_OK) {
            throw DBError(sqlite3_errmsg(db));
        }
    }

    SQLiteDatabase::SQLiteDatabase(const SQLiteDatabase& rhs)
    : path(rhs.path)
    {
        int status = 0;
        status = sqlite3_open(path.c_str(), &db);
        if (status != SQLITE_OK) {
            throw DBError(sqlite3_errmsg(db));
        }
    }

    SQLiteDatabase::~SQLiteDatabase()
    {
        int status = 0;
        status = sqlite3_close(db);
    }

    void SQLiteDatabase::createTable(const std::string& name, const std::vector<SQLColumn>& columns)
    {
        std::stringstream sqlss;
        sqlss << "CREATE TABLE " << name << " (";
        for (size_t i = 0; i < columns.size(); i++) {
            sqlss << columns.at(i).getSQLrepr();
            if (i < columns.size() - 1) {
                sqlss << ", ";
            }
        }
        sqlss << ");";
        std::string sql = sqlss.str();

        int status = sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
    }

    static inline int flexible_sqlite3_bind_value(sqlite3_stmt* stmt, int pos, unsigned long value)
    {
        return sqlite3_bind_int64(stmt, pos, value);
    }

    static inline int flexible_sqlite3_bind_value(sqlite3_stmt* stmt, int pos, double value)
    {
        return sqlite3_bind_double(stmt, pos, value);
    }

    template<typename T>
    void SQLiteDatabase::insertIntoTable(const std::string& name,
                                         const std::vector<std::vector<T>>& data)
    {
        int status = 0;

        size_t numRows = data.size();
        if (numRows == 0) return; //throw DBError("Input vector was empty");
        size_t numCols = data.at(0).size();

        std::stringstream sqlss;
        sqlss << "INSERT INTO " << name << " VALUES (";
        for (size_t i = 0; i < numCols - 1; i++) {
            sqlss << "?, ";
        }
        sqlss << "?);";
        std::string sql = sqlss.str();

        sqlite3_stmt* stmt = NULL;

        try {
            status = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

            status = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
            if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

            for (const auto& row : data) {
                if (row.size() != numCols) throw DBError("Incorrect number of items in row");
                for (size_t j = 0; j < numCols; j++) {
                    flexible_sqlite3_bind_value(stmt, j+1, row.at(j));
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

    template
    void SQLiteDatabase::insertIntoTable(const std::string&, const std::vector<std::vector<unsigned long>>&);

    template
    void SQLiteDatabase::insertIntoTable(const std::string&, const std::vector<std::vector<double>>&);

    arma::mat SQLiteDatabase::readTable(const std::string& name)
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
            arma::uword numRows = sqlite3_column_int64(rowcnt_stmt, 0);
            // std::cout << sqlite3_column_count(rowcnt_stmt) << std::endl;

            status = sqlite3_prepare_v2(db, select_sql.c_str(), -1, &select_stmt, NULL);
            if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

            arma::uword numCols = sqlite3_column_count(select_stmt);
            // std::cout << "Num rows = " << numRows << std::endl;
            // std::cout << "Num cols = " << numCols << std::endl;

            res.set_size(numRows, numCols);

            arma::uword i = 0;
            while (sqlite3_step(select_stmt) == SQLITE_ROW) {
                assert(i < numRows);
                for (arma::uword j = 0; j < numCols; j++) {
                    // std::cout << "at (" << i << ", " << j << ")" << std::endl;
                    res(i, j) = sqlite3_column_double(select_stmt, j);
                }
                i++;
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
