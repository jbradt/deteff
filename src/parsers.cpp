#include "parsers.h"

namespace YAML {
    Node convert<arma::vec>::encode(const arma::vec& rhs)
    {
        Node node;
        for (const auto item : rhs) {
            node.push_back(item);
        }
        return node;
    }

    bool convert<arma::vec>::decode(const Node& node, arma::vec& rhs)
    {
        if(!node.IsSequence()) return false;

        rhs.set_size(node.size());
        for (arma::uword i = 0; i < node.size(); i++) {
            rhs(i) = node[i].as<double>();
        }
        return true;
    }
}

std::vector<double> readEloss(const std::string& path)
{
    H5::H5File file (path.c_str(), H5F_ACC_RDONLY);
    H5::DataSet ds = file.openDataSet("eloss");

    H5::DataSpace filespace = ds.getSpace();
    int ndims = filespace.getSimpleExtentNdims();
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
    int ndims = filespace.getSimpleExtentNdims();
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

    // NOTE: Armadillo stores data in column-major order, while HDF5 uses
    // row-major ordering. Above, we read the data directly from HDF5 into
    // the arma matrix, so it was implicitly transposed. The next function
    // fixes this problem.
    arma::inplace_trans(res);
    return res;
}

XcfgParseResult parseXcfg(const std::string& path)
{
    pugi::xml_document doc;
    doc.load_file(path.c_str());

    XcfgParseResult results;
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
                auto starChannel = aget.node().select_node("channel[@id='*']");
                std::string defaultGain = starChannel.node().select_node("Gain").node().text().get();
                auto channels = aget.node().select_nodes("channel[@id!='*']");
                for (auto& channel : channels) {
                    std::string channelId = channel.node().attribute("id").value();
                    std::vector<int> addr = {stoi(coboId), stoi(asadId), stoi(agetId), stoi(channelId)};
                    // std::cout << "      " << channelId << std::endl;
                    auto trig = channel.node().select_node("TriggerInhibition");
                    if (trig) {
                        auto text = trig.node().text();
                        if (text && strcmp(text.get(), "inhibit_trigger") == 0) {
                            results.exclAddrs.push_back(addr);
                        }
                    }

                    auto gain = channel.node().select_node("Gain");
                    if (gain) {
                        auto text = gain.node().text();
                        if (text && strcmp(text.get(), defaultGain.c_str()) != 0) {
                            results.lowGainAddrs.push_back(addr);
                        }
                    }
                }
            }
        }
    }
    return results;
}