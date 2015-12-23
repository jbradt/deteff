#include "../mcopt/mcopt.h"
#include "PadMap.h"
#include "parsers.h"
#include "SQLiteWrapper.h"
#include <armadillo>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <tuple>

std::set<uint16_t> convertAddrsToPads(const std::vector<std::vector<int>>& addrs, const PadMap& padmap)
{
    std::set<uint16_t> pads;
    for (const auto& addr : addrs) {
        uint16_t pad = padmap.find(addr.at(0), addr.at(1), addr.at(2), addr.at(3));
        if (pad != padmap.missingValue) {
            pads.insert(pad);
        }
    }
    return pads;
}

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
    double tilt = config["tilt"].as<double>() * M_PI / 180;
    arma::vec beamCtr = config["beam_center"].as<arma::vec>() / 1000;  // Need to convert to meters

    // Read the energy loss data from the given path
    std::string elossPath = config["eloss_path"].as<std::string>();
    std::vector<double> eloss = readEloss(elossPath);

    // Read the pad mapping information. This includes the padmap, which maps
    // hardware address -> pad number, and the lookup table (LUT), which maps
    // position on the Micromegas to pad number.
    std::string lutPath = config["lut_path"].as<std::string>();
    double padRotAngle = config["pad_rot_angle"].as<double>() * M_PI / 180;
    arma::Mat<uint16_t> lut = readLUT(lutPath);
    PadPlane pads (lut, -0.280, 0.0001, -0.280, 0.0001, padRotAngle);
    std::string padmapPath = config["padmap_path"].as<std::string>();
    PadMap padmap (padmapPath);

    // Instantiate a minimizer so we can track particles.
    MCminimizer mcmin(massNum, chargeNum, eloss, efield, bfield);

    // Get the distribution parameters from the config file and create the parameters set
    arma::vec distMin = config["dist_min"].as<arma::vec>();
    distMin.rows(4, 5) *= M_PI / 180;
    arma::vec distMax = config["dist_max"].as<arma::vec>();
    distMax.rows(4, 5) *= M_PI / 180;
    arma::vec distCtr = distMin + (distMax - distMin) / 2;
    arma::vec distSig = (distMax - distMin);
    const unsigned distNumPts = config["dist_num_pts"].as<unsigned>();
    auto params = mcmin.makeParams(distCtr, distSig, distNumPts, distMin, distMax);

    // Parse the GET config file. This gives us the set of pads excluded from the trigger.
    std::string xcfgPath = config["xcfg_path"].as<std::string>();
    XcfgParseResult xcfgData = parseXcfg(xcfgPath);
    std::set<uint16_t> exclPads = convertAddrsToPads(xcfgData.exclAddrs, padmap);
    std::set<uint16_t> lowGainPads = convertAddrsToPads(xcfgData.lowGainAddrs, padmap);

    std::cout << "Number of excluded pads: " << exclPads.size() << std::endl;
    std::cout << "Number of low gain pads: " << lowGainPads.size() << std::endl;

    // Find the overall excluded pads, including low-gain and trigger-excluded
    std::set<uint16_t> badPads;
    std::set_union(exclPads.begin(), exclPads.end(),
                   lowGainPads.begin(), lowGainPads.end(),
                   std::inserter(badPads, badPads.begin()));

    std::cout << "Overall number of bad pads: " << badPads.size() << std::endl;

    // Create the SQL writer for output, and create the table for output within the database.
    // Then write the parameters to the database.
    sqlite::SQLiteDatabase db (outPath);
    std::vector<sqlite::SQLColumn> hitTableCols =
        {sqlite::SQLColumn("evt_id", "INTEGER"),
         sqlite::SQLColumn("hits", "INTEGER")};
    std::vector<sqlite::SQLColumn> hitPadsTableCols =
        {sqlite::SQLColumn("evt_id", "INTEGER"),
         sqlite::SQLColumn("pad", "INTEGER")};

    db.createTable("hit_counts", hitTableCols);
    db.createTable("hit_pads", hitPadsTableCols);

    

    // Iterate over the parameter sets, simulate each particle, and count the non-excluded pads
    // that were hit. Write this to the database.

    std::vector<std::pair<unsigned long, size_t>> results;
    std::vector<std::pair<unsigned long, std::set<uint16_t>>> hitPadsList;

    #pragma omp parallel private(results, hitPadsList)
    {
        #pragma omp for
        for (arma::uword i = 0; i < params.n_rows; i++) {
            auto tr = mcmin.trackParticle(params(i, 0), params(i, 1), params(i, 2), params(i, 3), params(i, 4),
                                          params(i, 5));
            tr.unTiltAndRecenter(beamCtr, tilt);  // Transforms from uvw (tilted) system to xyz (untilted) system
            std::set<uint16_t> hits = findHitPads(pads, tr, vd, clock);  // Includes uncalibration
            std::set<uint16_t> validHits;  // The pads that were hit and not excluded or low-gain
            std::set_difference(hits.begin(), hits.end(),
                                badPads.begin(), badPads.end(),
                                std::inserter(validHits, validHits.begin()));
            results.push_back(std::make_pair(i, validHits.size()));
            hitPadsList.push_back(std::make_pair(i, validHits));

            if (results.size() >= 1000) {
                #pragma omp critical
                {
                    std::cout << "Results length: " << results.size() << std::endl;
                }
                writer.writeResults(results);
                results.clear();
                writer.writeHitPads(hitPadsList);
                hitPadsList.clear();
            }
        }
        writer.writeResults(results);
        #pragma omp critical
        {
            std::cout << "Results length: " << results.size() << std::endl;
        }
    }

    return 0;
}
