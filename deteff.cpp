#include "../mcopt/mcopt.h"
#include "PadMap.h"
#include "parsers.h"
#include "SQLWriter.h"
#include <armadillo>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>


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
