#include "SQLWriter.h"
#include <cassert>

SQLWriter::SQLWriter(const std::string& path)
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

void SQLWriter::createTable()
{
    const std::string create_sql =
        "CREATE TABLE deteff (i INTEGER UNIQUE, x0 REAL, y0 REAL, z0 REAL, enu0 REAL, azi0 REAL, pol0 REAL, "
                             "trig INTEGER);";
    int status = sqlite3_exec(db, create_sql.c_str(), NULL, NULL, NULL);
    if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
}

void SQLWriter::writeParameters(const arma::mat& params)
{
    assert(params.n_cols == 6);

    int status = 0;
    sqlite3_stmt* insert_stmt = NULL;
    const std::string insert_sql =
    "INSERT INTO deteff VALUES (:i, :x0, :y0, :z0, :enu0, :azi0, :pol0, :trig);";

    try {
        status = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

        status = sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt, NULL);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

        for (arma::uword i = 0; i < params.n_rows; i++) {
            // Assume params has columns x, y, z, en, azi, pol
            status = sqlite3_bind_int(insert_stmt, 1, i);
            if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

            for (arma::uword j = 0; j < params.n_cols; j++) {
                sqlite3_bind_double(insert_stmt, j+2, params(i, j));
                if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
            }
            status = sqlite3_step(insert_stmt);
            if (status != SQLITE_DONE) {
                std::cerr << "Insertion failed: " << sqlite3_errmsg(db) << std::endl;
            }
            sqlite3_reset(insert_stmt);
            sqlite3_clear_bindings(insert_stmt);
        }
    }
    catch (const DBError& e) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_finalize(insert_stmt);
        insert_stmt = NULL;
        throw;
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(insert_stmt);
    insert_stmt = NULL;
}

void SQLWriter::writeResult(const arma::uword idx, const int trig)
{
    int status = 0;
    const std::string update_sql = "UPDATE deteff SET trig=? WHERE i=?;";
    sqlite3_stmt* update_stmt = NULL;

    try {
        status = sqlite3_prepare_v2(db, update_sql.c_str(), -1, &update_stmt, NULL);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

        status = sqlite3_bind_int(update_stmt, 2, idx);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
        status = sqlite3_bind_int(update_stmt, 1, trig);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

        status = sqlite3_step(update_stmt);
        if (status != SQLITE_DONE) throw DBError(sqlite3_errmsg(db));
    }
    catch (const DBError& e) {
        sqlite3_finalize(update_stmt);
        update_stmt = NULL;
        throw;
    }

    sqlite3_finalize(update_stmt);
    update_stmt = NULL;
}
