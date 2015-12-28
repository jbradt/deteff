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
#include <unordered_map>

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

std::unordered_map<uint16_t, uint8_t> makeCoBoMap(const PadMap& pm)
{
    std::unordered_map<uint16_t, uint8_t> res;
    for (uint8_t cobo = 0; cobo < 10; cobo++) {
        for (uint8_t asad = 0; asad < 4; asad++) {
            for (uint8_t aget = 0; aget < 4; aget++) {
                for (uint16_t channel = 0; channel < 68; channel++) {
                    uint16_t pad = pm.find(cobo, asad, aget, channel);
                    if (pad != pm.missingValue) {
                        res.emplace(pad, cobo);
                    }
                }
            }
        }
    }
    return res;
}

static auto restructureResults(const std::vector<std::pair<unsigned long, std::set<uint16_t>>>& res,
                               const std::unordered_map<uint16_t, uint8_t>& cobomap)
{
    std::vector<std::vector<unsigned long>> hitCountsRows;
    std::vector<std::vector<unsigned long>> hitPadsRows;
    std::vector<std::vector<unsigned long>> coboHitsRows;
    for (const auto& pair : res) {
        // pairs in res are (evt id, set of pad numbers)
        const auto& evt_id = pair.first;
        const auto& hitpads = pair.second;
        std::vector<unsigned long> coboHits (11, 0);
        coboHits.at(0) = evt_id;

        hitCountsRows.push_back({evt_id, hitpads.size()});
        for (const auto pad : hitpads) {
            hitPadsRows.push_back({evt_id, pad});
            auto coboMapElement = cobomap.find(pad);
            if (coboMapElement != cobomap.end()) {
                coboHits.at(coboMapElement->second + 1)++;
            }
        }
        coboHitsRows.push_back(coboHits);
    }
    return std::make_tuple(hitCountsRows, hitPadsRows, coboHitsRows);
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
    auto cobomap = makeCoBoMap(padmap);

    // Instantiate a minimizer so we can track particles.
    MCminimizer mcmin(massNum, chargeNum, eloss, efield, bfield);

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

    // Open the SQLite database and read the parameters table
    sqlite::SQLiteDatabase db (outPath);
    arma::mat params = db.readTable("params");
    std::cout << "Found params table with " << params.n_rows << " rows" << std::endl;

    // Now create tables for output.
    std::string hitCountsTableName = "hit_counts";
    std::vector<sqlite::SQLColumn> hitCountsTableCols =
        {sqlite::SQLColumn("evt_id", "INTEGER"),
         sqlite::SQLColumn("hits", "INTEGER")};
    db.createTable(hitCountsTableName, hitCountsTableCols);

    std::string hitPadsTableName = "hit_pads";
    std::vector<sqlite::SQLColumn> hitPadsTableCols =
        {sqlite::SQLColumn("evt_id", "INTEGER"),
         sqlite::SQLColumn("pad", "INTEGER")};
    db.createTable(hitPadsTableName, hitPadsTableCols);

    std::string coboHitsTableName = "cobo_hits";
    std::vector<sqlite::SQLColumn> coboHitsTableCols =
        {sqlite::SQLColumn("evt_id", "INTEGER"),
         sqlite::SQLColumn("cobo0", "INTEGER"),
         sqlite::SQLColumn("cobo1", "INTEGER"),
         sqlite::SQLColumn("cobo2", "INTEGER"),
         sqlite::SQLColumn("cobo3", "INTEGER"),
         sqlite::SQLColumn("cobo4", "INTEGER"),
         sqlite::SQLColumn("cobo5", "INTEGER"),
         sqlite::SQLColumn("cobo6", "INTEGER"),
         sqlite::SQLColumn("cobo7", "INTEGER"),
         sqlite::SQLColumn("cobo8", "INTEGER"),
         sqlite::SQLColumn("cobo9", "INTEGER")};
    db.createTable(coboHitsTableName, coboHitsTableCols);

    // Iterate over the parameter sets, simulate each particle, and count the non-excluded pads
    // that were hit. Write this to the database.

    std::vector<std::pair<unsigned long, std::set<uint16_t>>> results;
    std::vector<std::vector<unsigned long>> hitCountsRows;
    std::vector<std::vector<unsigned long>> hitPadsRows;
    std::vector<std::vector<unsigned long>> coboHitsRows;

    #pragma omp parallel private(results, hitCountsRows, hitPadsRows, coboHitsRows)
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
            results.push_back(std::make_pair(i, validHits));

            if (results.size() >= 1000) {
                #pragma omp critical
                {
                    std::cout << "Results length: " << results.size() << std::endl;
                }
                std::tie(hitCountsRows, hitPadsRows, coboHitsRows) = restructureResults(results, cobomap);
                assert(hitCountsRows.size() == results.size());

                db.insertIntoTable(hitCountsTableName, hitCountsRows);
                db.insertIntoTable(hitPadsTableName, hitPadsRows);
                db.insertIntoTable(coboHitsTableName, coboHitsRows);

                results.clear();
            }
        }
        std::tie(hitCountsRows, hitPadsRows, coboHitsRows) = restructureResults(results, cobomap);

        db.insertIntoTable(hitCountsTableName, hitCountsRows);
        db.insertIntoTable(hitPadsTableName, hitPadsRows);
        db.insertIntoTable(coboHitsTableName, coboHitsRows);
        #pragma omp critical
        {
            std::cout << "Results length: " << results.size() << std::endl;
        }
    }

    return 0;
}
