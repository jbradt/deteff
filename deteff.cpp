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

#ifdef _OPENMP
    #include <omp.h>
#endif /* def _OPENMP */

struct setMapCmp
{
    bool operator()(int i, const std::pair<int, int>& p) const { return i < p.first; }
    bool operator()(const std::pair<int, int>& p, int i) const { return p.first < i; }
};

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

static auto restructureResults(const std::vector<std::pair<unsigned long, std::map<uint16_t, unsigned long>>>& res,
                               const std::unordered_map<uint16_t, uint8_t>& cobomap)
{
    std::vector<std::vector<unsigned long>> hitsRows;
    for (const auto& pair : res) {
        // pairs in res are (evt id, map of pad->hits)
        const auto& evt_id = pair.first;
        const auto& hitmap = pair.second;

        for (const auto& entry : hitmap) {
            const auto& pad = entry.first;
            const auto& num_elec = entry.second;
            auto coboMapElement = cobomap.find(pad);
            if (coboMapElement != cobomap.end()) {
                hitsRows.push_back({evt_id, coboMapElement->second, pad, num_elec});
            }
        }
    }
    return hitsRows;
}

int main(const int argc, const char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage: deteff CONFIG_PATH ELOSS_PATH OUTPUT_PATH" << std::endl;
        return 1;
    }

    const std::string configPath = argv[1];
    const std::string elossPath = argv[2];
    const std::string outPath = argv[3];

    // Parse config file
    YAML::Node config = YAML::LoadFile(configPath);

    // Get values of physics parameters from config file
    arma::vec efield = config["efield"].as<arma::vec>();
    arma::vec bfield = config["bfield"].as<arma::vec>();
    unsigned massNum = config["mass_num"].as<unsigned>();
    unsigned chargeNum = config["charge_num"].as<unsigned>();
    double ioniz = config["ioniz"].as<double>();
    arma::vec vd = config["vd"].as<arma::vec>();
    double clock = config["clock"].as<double>();
    double tilt = config["tilt"].as<double>() * M_PI / 180;
    arma::vec beamCtr = config["beam_center"].as<arma::vec>() / 1000;  // Need to convert to meters

    // Read the energy loss data from the given path
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

    std::string hitsTableName = "hits";
    std::vector<sqlite::SQLColumn> hitsTableCols =
        {sqlite::SQLColumn("evt_id", "INTEGER"),
         sqlite::SQLColumn("cobo", "INTEGER"),
         sqlite::SQLColumn("pad", "INTEGER"),
         sqlite::SQLColumn("num_elec", "INTEGER")};
    db.createTable(hitsTableName, hitsTableCols);

    // Iterate over the parameter sets, simulate each particle, and count the non-excluded pads
    // that were hit. Write this to the database.


    #pragma omp parallel
    {
        #ifdef _OPENMP
            int threadNum = omp_get_thread_num();
        #else
            int threadNum = 0;
        #endif /* def _OPENMP */
        
        std::vector<std::pair<unsigned long, std::map<uint16_t, unsigned long>>> results;
        std::vector<std::vector<unsigned long>> hitsRows;

        #pragma omp for schedule(runtime)
        for (arma::uword i = 0; i < params.n_rows; i++) {
            auto tr = mcmin.trackParticle(params(i, 0), params(i, 1), params(i, 2), params(i, 3), params(i, 4),
                                          params(i, 5));
            tr.unTiltAndRecenter(beamCtr, tilt);  // Transforms from uvw (tilted) system to xyz (untilted) system
            auto hits = findHitPads(pads, tr, vd, clock, massNum, ioniz);  // Includes uncalibration
            decltype(hits) validHits;  // The pads that were hit and not excluded or low-gain
            std::set_difference(hits.begin(), hits.end(),
                                badPads.begin(), badPads.end(),
                                std::inserter(validHits, validHits.begin()), setMapCmp());
            results.push_back(std::make_pair(i, validHits));

            if (results.size() >= 1000 + threadNum*100) {
                hitsRows = restructureResults(results, cobomap);
                #pragma omp critical
                {
                    db.insertIntoTable(hitsTableName, hitsRows);
                    std::cout << "Thread " << threadNum << " wrote " << results.size() << std::endl;
                }
                results.clear();
            }
        }
        hitsRows = restructureResults(results, cobomap);

        #pragma omp critical
        {
            db.insertIntoTable(hitsTableName, hitsRows);
            std::cout << "Thread " << threadNum << " wrote " << results.size() << std::endl;
        }
    }

    return 0;
}
