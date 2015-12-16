#ifndef SQLWRITER_H
#define SQLWRITER_H

#include <sqlite3.h>
#include <exception>
#include <string>
#include <armadillo>

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
    ~SQLWriter();

    void createTable();
    void writeParameters(const arma::mat& params);
    void writeResult(const arma::uword idx, const int trig);

private:
    sqlite3* db;
};

#endif /* def SQLWRITER_H */
