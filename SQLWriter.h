#ifndef SQLWRITER_H
#define SQLWRITER_H

#include <sqlite3.h>
#include <exception>
#include <string>
#include <armadillo>
#include <vector>
#include <utility>

class DBError : public std::exception
{
public:
    DBError(const std::string& m) : msg(m) {}
    const char* what() const noexcept { return msg.c_str(); }

private:
    std::string msg;
};

class SQLWriter
{
public:
    SQLWriter(const std::string& path);
    SQLWriter(const SQLWriter& rhs);
    ~SQLWriter();

    void createTable();
    void writeParameters(const arma::mat& params);
    void writeResult(const arma::uword idx, const unsigned long numHit);
    void writeResults(const std::vector<std::pair<unsigned long, size_t>>& results);

private:
    const std::string path;
    sqlite3* db;
};

#endif /* def SQLWRITER_H */
