#ifndef SQLITEDB_H
#define SQLITEDB_H

#include <sqlite3.h>
#include <exception>
#include <string>
#include <armadillo>
#include <vector>
#include <utility>
#include <set>
#include <sstream>

namespace sqlite {

    class DBError : public std::exception
    {
    public:
        DBError(const std::string& m) { msg = "DBError: " + m; }
        const char* what() const noexcept { return msg.c_str(); }

    private:
        std::string msg;
    };

    class SQLColumn
    {
    public:
        std::string name;
        std::string type;
        std::string constraints;

        SQLColumn(const std::string& n, const std::string& t, const std::string& c="") : name(n), type(t), constraints(c) {}
        std::string getSQLrepr() const { return name + " " + type + " " + constraints; }
    };

    class SQLiteDatabase
    {
    public:
        SQLiteDatabase(const std::string& path);
        SQLiteDatabase(const SQLiteDatabase& rhs);
        ~SQLiteDatabase();

        void createTable(const std::string& name, const std::vector<SQLColumn>& columns);
        template<typename T>
        void insertIntoTable(const std::string& name,
                             const std::vector<std::vector<T>>& data);
        arma::mat readTable(const std::string& name);

    private:
        const std::string path;
        sqlite3* db;
    };

}

#endif /* def SQLITEDB_H */
