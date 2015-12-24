namespace sqlite {
    static inline int flexible_sqlite3_bind_value(sqlite3_stmt* stmt, int pos, int value)
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
        if (numRows == 0) throw DBError("Input vector was empty");
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
}
