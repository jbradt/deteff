#include "../mcopt/mcopt.h"
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
#include <chrono>

#ifdef _OPENMP
    #include <omp.h>
#endif /* def _OPENMP */

struct setMapCmp
{
    bool operator()(int i, const std::pair<mcopt::pad_t, mcopt::Peak>& p) const { return i < p.first; }
    bool operator()(const std::pair<mcopt::pad_t, mcopt::Peak>& p, int i) const { return p.first < i; }
};

std::set<uint16_t> convertAddrsToPads(const std::vector<std::vector<int>>& addrs, const mcopt::PadMap& padmap)
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

static auto restructureResults(const std::vector<std::pair<unsigned long, std::map<uint16_t, mcopt::Peak>>>& res,
                               const mcopt::PadMap& padmap)
{
    std::vector<std::vector<unsigned long>> hitsRows;
    for (const auto& pair : res) {
        // pairs in res are (evt id, map of pad->hits)
        const auto& evt_id = pair.first;
        const auto& hitmap = pair.second;

        for (const auto& entry : hitmap) {
            const auto& pad = entry.first;
            const auto& peak = entry.second;
            unsigned cobo = padmap.reverseFind(pad).cobo;
            if (cobo != padmap.missingValue) {
                hitsRows.push_back({evt_id, cobo, pad, peak.timeBucket, peak.amplitude});
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
    double shape = config["shape"].as<double>();
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
    mcopt::PadPlane pads (lut, -0.280, 0.0001, -0.280, 0.0001, padRotAngle);
    std::string padmapPath = config["padmap_path"].as<std::string>();
    mcopt::PadMap padmap (padmapPath);

    // Prepare the trigger object
    unsigned padThreshMSB = config["pad_thresh_MSB"].as<unsigned>();
    unsigned padThreshLSB = config["pad_thresh_LSB"].as<unsigned>();
    double trigWidth = config["trigger_signal_width"].as<double>();
    unsigned long multThresh = config["multiplicity_threshold"].as<unsigned long>();
    unsigned long multWindow = config["multiplicity_window"].as<unsigned long>();
    double gain = config["gain"].as<double>();
    double discrFrac = config["trigger_discriminator_fraction"].as<double>();
    unsigned umegasGain = config["micromegas_gain"].as<unsigned>();
    mcopt::Trigger trigger (padThreshMSB, padThreshLSB, trigWidth, multThresh, multWindow, clock,
                            gain, discrFrac, padmap);

    std::cout << "Trigger threshold: " << trigger.getPadThresh() << " electrons" << std::endl;
    std::cout << "Multiplicity window: " << trigger.getMultWindow() << " time buckets" << std::endl;

    // Instantiate a tracker so we can track particles.
    mcopt::Tracker tracker(massNum, chargeNum, eloss, efield, bfield);

    // Create an EventGenerator to process the track into signals and peaks
    mcopt::EventGenerator evtgen(pads, vd, clock, shape, massNum, ioniz, umegasGain);

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
         sqlite::SQLColumn("tb", "INTEGER"),
         sqlite::SQLColumn("num_elec", "INTEGER")};
    db.createTable(hitsTableName, hitsTableCols);
    db.createIndex(hitsTableName, "evt_id");

    std::string trigTableName = "trig";
    std::vector<sqlite::SQLColumn> trigTableCols =
        {sqlite::SQLColumn("evt_id", "INTEGER PRIMARY KEY"),
         sqlite::SQLColumn("trig", "INTEGER")};
    db.createTable(trigTableName, trigTableCols);

    // Iterate over the parameter sets, simulate each particle, and count the non-excluded pads
    // that were hit. Write this to the database.

    unsigned numFinished = 0;

    #pragma omp parallel shared(numFinished)
    {
        #ifdef _OPENMP
            int threadNum = omp_get_thread_num();
        #else
            int threadNum = 0;
        #endif /* def _OPENMP */

        std::vector<std::pair<unsigned long, std::map<uint16_t, mcopt::Peak>>> results;
        std::vector<std::vector<unsigned long>> trigRows;
        std::vector<std::vector<unsigned long>> hitsRows;
        std::vector<std::chrono::steady_clock::duration> times;

        #pragma omp for schedule(runtime)
        for (arma::uword i = 0; i < params.n_rows; i++) {

            auto begin = std::chrono::steady_clock::now();
            auto tr = tracker.trackParticle(params(i, 0), params(i, 1), params(i, 2), params(i, 3), params(i, 4),
                                            params(i, 5));
            tr.unTiltAndRecenter(beamCtr, tilt);  // Transforms from uvw (tilted) system to xyz (untilted) system
            auto hits = evtgen.makePeaksFromSimulation(tr);  // Includes uncalibration

            decltype(hits) validHits;  // The pads that were hit and not excluded or low-gain
            std::set_difference(hits.begin(), hits.end(),
                                badPads.begin(), badPads.end(),
                                std::inserter(validHits, validHits.begin()), setMapCmp());

            bool trigRes = trigger.didTrigger(validHits);
            trigRows.push_back({i, trigRes});

            results.push_back(std::make_pair(i, validHits));

            auto end = std::chrono::steady_clock::now();
            times.push_back(end-begin);

            if (results.size() >= 1000 + threadNum*100) {
                hitsRows = restructureResults(results, padmap);
                auto total = std::accumulate(times.begin(), times.end(), std::chrono::steady_clock::duration::zero());
                auto timePerIter = std::chrono::duration_cast<std::chrono::microseconds>(total / times.size());
                #pragma omp critical
                {
                    db.insertIntoTable(hitsTableName, hitsRows);
                    db.insertIntoTable(trigTableName, trigRows);
                    numFinished += results.size();
                    std::cout << "(Thread " << threadNum << ") "
                              << numFinished << "/" << params.n_rows << " events. ";
                    std::cout << "(" << timePerIter.count() << " us/event)" << std::endl;
                }
                results.clear();
                trigRows.clear();
                times.clear();
            }
        }
        hitsRows = restructureResults(results, padmap);
        auto total = std::accumulate(times.begin(), times.end(), std::chrono::steady_clock::duration::zero());
        auto timePerIter = std::chrono::duration_cast<std::chrono::microseconds>(total / times.size());

        #pragma omp critical
        {
            db.insertIntoTable(hitsTableName, hitsRows);
            db.insertIntoTable(trigTableName, trigRows);
            numFinished += results.size();
            std::cout << "(Thread " << threadNum << ") "
                      << numFinished << "/" << params.n_rows << " events. ";
            std::cout << "(" << timePerIter.count() << " us/event)" << std::endl;
        }
    }

    return 0;
}
