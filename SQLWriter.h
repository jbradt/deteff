#ifndef SQLWRITER_H
#define SQLWRITER_H

#include <sqlite3.h>
#include <exception>
#include <string>
#include <armadillo>
#include <vector>
#include <utility>
#include <set>
#include <sstream>

class DBError : public std::exception
{
public:
    DBError(const std::string& m) : msg(m) {}
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

class SQLWriter
{
public:
    SQLWriter(const std::string& path);
    SQLWriter(const SQLWriter& rhs);
    ~SQLWriter();

    void createTable(const std::string& name, const std::vector<SQLColumn>& columns);
    void insertIntoTable(const std::string& name, const arma::mat& data);

    void writeParameters(const arma::mat& params);
    void writeResult(const arma::uword idx, const unsigned long numHit);
    void writeResults(const std::vector<std::pair<unsigned long, size_t>>& results);

    void createPadTable();
    void writeHitPads(const std::vector<std::pair<unsigned long, std::set<uint16_t>>>& hits);

private:
    const std::string path;
    sqlite3* db;
};

#endif /* def SQLWRITER_H */
