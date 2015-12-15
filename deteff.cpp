#include "../mcopt/mcopt.h"
#include "PadMap.h"
#include <armadillo>
#include <yaml-cpp/yaml.h>
#include <H5Cpp.h>
#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <tuple>
#include <set>
#include <sqlite3.h>
#include <pugixml.hpp>
#include <algorithm>

class MissingConfigKey : public std::exception
{
public:
    MissingConfigKey(std::string m) : msg(m) {}
    const char* what() const noexcept { return msg.c_str(); }

private:
    std::string msg;
};

namespace YAML {
    template<>
    struct convert<arma::vec> {
        static Node encode(const arma::vec& rhs) {
            Node node;
            for (const auto item : rhs) {
                node.push_back(item);
            }
            return node;
        }

        static bool decode(const Node& node, arma::vec& rhs) {
            if(!node.IsSequence()) return false;

            rhs.set_size(node.size());
            for (int i = 0; i < node.size(); i++) {
                rhs(i) = node[i].as<double>();
            }
            return true;
        }
    };
}

std::vector<double> readEloss(const std::string& path)
{
    H5::H5File file (path.c_str(), H5F_ACC_RDONLY);
    H5::DataSet ds = file.openDataSet("eloss");

    H5::DataSpace filespace = ds.getSpace();
    hsize_t ndims = filespace.getSimpleExtentNdims();
    assert(ndims == 1);
    hsize_t dim;
    filespace.getSimpleExtentDims(&dim);

    H5::DataSpace memspace (ndims, &dim);

    std::vector<double> res (dim);

    ds.read(res.data(), H5::PredType::NATIVE_DOUBLE, memspace, filespace);

    filespace.close();
    memspace.close();
    ds.close();
    file.close();
    return res;
}

arma::Mat<uint16_t> readLUT(const std::string& path)
{
    H5::H5File file (path.c_str(), H5F_ACC_RDONLY);
    H5::DataSet ds = file.openDataSet("LUT");

    H5::DataSpace filespace = ds.getSpace();
    hsize_t ndims = filespace.getSimpleExtentNdims();
    assert(ndims == 2);
    hsize_t dims[2] = {1, 1};
    filespace.getSimpleExtentDims(dims);

    H5::DataSpace memspace (ndims, dims);

    arma::Mat<uint16_t> res (dims[0], dims[1]);

    ds.read(res.memptr(), H5::PredType::NATIVE_UINT16, memspace, filespace);

    filespace.close();
    memspace.close();
    ds.close();
    file.close();
    return res;
}

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

std::vector<std::vector<int>> parseXcfg(const std::string& path)
{
    pugi::xml_document doc;
    doc.load_file(path.c_str());

    std::vector<std::vector<int>> result;

    auto cobos = doc.select_nodes("//Node[@id='CoBo']/Instance[@id!='*']");

    for (auto& cobo : cobos) {
        std::string coboId = cobo.node().attribute("id").value();
        // std::cout << coboId << std::endl;
        auto asads = cobo.node().select_nodes("AsAd[@id!='*']");
        for (auto& asad : asads) {
            std::string asadId = asad.node().attribute("id").value();
                // std::cout << "  " << asadId << std::endl;
            auto agets = asad.node().select_nodes("Aget[@id!='*']");
            for (auto& aget : agets) {
                std::string agetId = aget.node().attribute("id").value();
                // std::cout << "    " << agetId << std::endl;
                auto channels = aget.node().select_nodes("channel[@id!='*']");
                for (auto& channel : channels) {
                    std::string channelId = channel.node().attribute("id").value();
                    // std::cout << "      " << channelId << std::endl;
                    auto trig = channel.node().select_node("TriggerInhibition");
                    if (trig) {
                        auto text = trig.node().text();
                        if (text && strcmp(text.get(), "inhibit_trigger") == 0) {
                            std::vector<int> addr = {stoi(coboId), stoi(asadId), stoi(agetId), stoi(channelId)};
                            result.push_back(addr);
                        }
                    }
                }
            }
        }
    }
    return result;
}

int main(const int argc, const char** argv)
{
    if (argc < 3) {
        std::cerr << "Need path to config file and output file." << std::endl;
        return 1;
    }

    const std::string configPath = argv[1];
    const std::string outPath = argv[2];

    YAML::Node config = YAML::LoadFile(configPath);

    arma::vec efield = config["efield"].as<arma::vec>();
    arma::vec bfield = config["bfield"].as<arma::vec>();
    std::string elossPath = config["eloss_path"].as<std::string>();
    std::string lutPath = config["lut_path"].as<std::string>();
    std::string xcfgPath = config["xcfg_path"].as<std::string>();
    std::string padmapPath = config["padmap_path"].as<std::string>();
    unsigned massNum = config["mass_num"].as<unsigned>();
    unsigned chargeNum = config["charge_num"].as<unsigned>();
    arma::vec vd = config["vd"].as<arma::vec>();
    double clock = config["clock"].as<double>();

    std::vector<double> eloss = readEloss(elossPath);
    arma::Mat<uint16_t> lut = readLUT(lutPath);
    PadMap padmap (padmapPath);

    auto exclAddrs = parseXcfg(xcfgPath);
    std::set<uint16_t> exclPads;
    for (const auto& addr : exclAddrs) {
        uint16_t pad = padmap.find(addr.at(0), addr.at(1), addr.at(2), addr.at(3));
        if (pad != padmap.missingValue) {
            exclPads.insert(pad);
        }
    }

    PadPlane pads (lut, -0.280, 0.0001, -0.280, 0.0001);

    MCminimizer mcmin(massNum, chargeNum, eloss, efield, bfield);

    SQLWriter writer (outPath);
    writer.createTable();

    arma::vec ctr   = {0, 0, 0.5, 2, M_PI, 3*M_PI/4};
    arma::vec sigma = {0, 0, 1.0, 2, 2*M_PI, 3*M_PI/4};
    arma::vec mins = ctr - sigma/2;
    arma::vec maxes = ctr + sigma/2;
    auto params = mcmin.makeParams(ctr, sigma, 100, mins, maxes);

    writer.writeParameters(params);

    for (arma::uword i = 0; i < params.n_rows; i++) {
        auto tr = mcmin.trackParticle(params(i, 0), params(i, 1), params(i, 2), params(i, 3), params(i, 4),
                                      params(i, 5));
        std::set<uint16_t> hits = findHitPads(pads, tr, vd, clock);
        std::set<uint16_t> commonHits;
        std::set_difference(hits.begin(), hits.end(),
                            exclPads.begin(), exclPads.end(),
                            std::inserter(commonHits, commonHits.begin()));

        for (auto p : commonHits) {
            std::cout << p << ", ";
        }
        std::cout << std::endl;
        bool result = commonHits.size() > 1;
        writer.writeResult(i, result);
    }

    return 0;
}
