#include "SQLWriter.h"
#include <cassert>

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

// void SQLWriter::createTable()
// {
//     const std::string create_sql =
//         "CREATE TABLE deteff (i INTEGER UNIQUE, x0 REAL, y0 REAL, z0 REAL, enu0 REAL, azi0 REAL, pol0 REAL, "
//                              "numHit INTEGER);";
//     int status = sqlite3_exec(db, create_sql.c_str(), NULL, NULL, NULL);
//     if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
// }

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
    sqlss << "INSERT INTO " << name << " VALUES (";
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

        for (arma::uword i = 0; i < data.n_cols; i++) {

        }
    }
}

void SQLWriter::writeParameters(const arma::mat& params)
{
    assert(params.n_cols == 6);

    int status = 0;
    sqlite3_stmt* insert_stmt = NULL;
    const std::string insert_sql =
    "INSERT INTO deteff VALUES (:i, :x0, :y0, :z0, :enu0, :azi0, :pol0, :numHit);";

    try {
        status = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

        status = sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt, NULL);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

        for (arma::uword i = 0; i < params.n_rows; i++) {
            // Assume params has columns x, y, z, en, azi, pol
            status = sqlite3_bind_int64(insert_stmt, 1, i);
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
    catch (const DBError&) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_finalize(insert_stmt);
        insert_stmt = NULL;
        throw;
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(insert_stmt);
    insert_stmt = NULL;
}

void SQLWriter::writeResult(const arma::uword idx, const unsigned long numHit)
{
    int status = 0;
    const std::string update_sql = "UPDATE deteff SET numHit=? WHERE i=?;";
    sqlite3_stmt* update_stmt = NULL;

    try {
        status = sqlite3_prepare_v2(db, update_sql.c_str(), -1, &update_stmt, NULL);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

        status = sqlite3_bind_int64(update_stmt, 2, idx);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
        status = sqlite3_bind_int64(update_stmt, 1, numHit);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

        status = sqlite3_step(update_stmt);
        if (status != SQLITE_DONE) throw DBError(sqlite3_errmsg(db));
    }
    catch (const DBError&) {
        sqlite3_finalize(update_stmt);
        update_stmt = NULL;
        throw;
    }

    sqlite3_finalize(update_stmt);
    update_stmt = NULL;
}

void SQLWriter::writeResults(const std::vector<std::pair<unsigned long, size_t>>& results)
{
    int status = 0;
    sqlite3_stmt* update_stmt = NULL;
    const std::string update_sql =
        "UPDATE deteff SET numHit=? WHERE i=?;";

    try {
        status = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

        status = sqlite3_prepare_v2(db, update_sql.c_str(), -1, &update_stmt, NULL);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

        for (const auto& res : results) {
            status = sqlite3_bind_int64(update_stmt, 2, res.first);
            if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
            status = sqlite3_bind_int64(update_stmt, 1, res.second);
            if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

            status = sqlite3_step(update_stmt);
            if (status != SQLITE_DONE) {
                std::cerr << "Insertion failed: " << sqlite3_errmsg(db) << std::endl;
            }
            sqlite3_reset(update_stmt);
            sqlite3_clear_bindings(update_stmt);
        }
    }
    catch (const DBError&) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_finalize(update_stmt);
        update_stmt = NULL;
        throw;
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(update_stmt);
    update_stmt = NULL;
}

void SQLWriter::createPadTable()
{
    const std::string create_sql =
        "CREATE TABLE hitpads (i INTEGER, pad INTEGER);";
    int status = sqlite3_exec(db, create_sql.c_str(), NULL, NULL, NULL);
    if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
}

void SQLWriter::writeHitPads(const std::vector<std::pair<unsigned long, std::set<uint16_t>>>& hits)
{
    int status = 0;
    sqlite3_stmt* insert_stmt = NULL;
    const std::string insert_sql =
        "INSERT INTO hitpads VALUES (?, ?);";

    try {
        status = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

        status = sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt, NULL);
        if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

        for (const auto& item : hits) {
            unsigned long iterNum = item.first;
            std::set<uint16_t> pads = item.second;
            for (const uint16_t pad : pads) {
                status = sqlite3_bind_int64(insert_stmt, 1, iterNum);
                if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));
                status = sqlite3_bind_int(insert_stmt, 2, pad);
                if (status != SQLITE_OK) throw DBError(sqlite3_errmsg(db));

                status = sqlite3_step(insert_stmt);
                if (status != SQLITE_DONE) {
                    std::cerr << "Insertion failed: " << sqlite3_errmsg(db) << std::endl;
                }
                sqlite3_reset(insert_stmt);
                sqlite3_clear_bindings(insert_stmt);
            }
        }
    }
    catch (const DBError&) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_finalize(insert_stmt);
        insert_stmt = NULL;
        throw;
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(insert_stmt);
    insert_stmt = NULL;
}
