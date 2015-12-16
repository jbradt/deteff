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

    // Parse config file
    YAML::Node config = YAML::LoadFile(configPath);

    // Get values of physics parameters from config file
    arma::vec efield = config["efield"].as<arma::vec>();
    arma::vec bfield = config["bfield"].as<arma::vec>();
    unsigned massNum = config["mass_num"].as<unsigned>();
    unsigned chargeNum = config["charge_num"].as<unsigned>();
    arma::vec vd = config["vd"].as<arma::vec>();
    double clock = config["clock"].as<double>();

    // Read the energy loss data from the given path
    std::string elossPath = config["eloss_path"].as<std::string>();
    std::vector<double> eloss = readEloss(elossPath);

    // Read the pad mapping information. This includes the padmap, which maps
    // hardware address -> pad number, and the lookup table (LUT), which maps
    // position on the Micromegas to pad number.
    std::string lutPath = config["lut_path"].as<std::string>();
    arma::Mat<uint16_t> lut = readLUT(lutPath);
    PadPlane pads (lut, -0.280, 0.0001, -0.280, 0.0001);
    std::string padmapPath = config["padmap_path"].as<std::string>();
    PadMap padmap (padmapPath);

    // Instantiate a minimizer so we can track particles.
    MCminimizer mcmin(massNum, chargeNum, eloss, efield, bfield);

    // Get the distribution parameters from the config file and create the parameters set
    arma::vec distMin = config["dist_min"].as<arma::vec>();
    distMin.rows(4, 5) *= M_PI / 180;
    arma::vec distMax = config["dist_max"].as<arma::vec>();
    distMax.rows(4, 5) *= M_PI / 180;
    arma::vec distCtr = (distMax - distMin) / 2;
    arma::vec distSig = (distMax - distMin);
    const unsigned distNumPts = config["dist_num_pts"].as<unsigned>();
    auto params = mcmin.makeParams(distCtr, distSig, distNumPts, distMin, distMax);

    // Parse the GET config file. This gives us the set of pads excluded from the trigger.
    std::string xcfgPath = config["xcfg_path"].as<std::string>();
    auto exclAddrs = parseXcfg(xcfgPath);
    std::set<uint16_t> exclPads;
    for (const auto& addr : exclAddrs) {
        uint16_t pad = padmap.find(addr.at(0), addr.at(1), addr.at(2), addr.at(3));
        if (pad != padmap.missingValue) {
            exclPads.insert(pad);
        }
    }

    // Create the SQL writer for output, and create the table for output within the database.
    // Then write the parameters to the database.
    SQLWriter writer (outPath);
    writer.createTable();
    writer.writeParameters(params);

    // Iterate over the parameter sets, simulate each particle, and count the non-excluded pads
    // that were hit. Write this to the database.
    for (arma::uword i = 0; i < params.n_rows; i++) {
        auto tr = mcmin.trackParticle(params(i, 0), params(i, 1), params(i, 2), params(i, 3), params(i, 4),
                                      params(i, 5));
        std::set<uint16_t> hits = findHitPads(pads, tr, vd, clock);
        std::set<uint16_t> validHits;  // The pads that were hit and not excluded
        std::set_difference(hits.begin(), hits.end(),
                            exclPads.begin(), exclPads.end(),
                            std::inserter(validHits, validHits.begin()));
        writer.writeResult(i, validHits.size());
    }

    return 0;
}
