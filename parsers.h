#ifndef PARSERS_H
#define PARSERS_H

#include <yaml-cpp/yaml.h>
#include <H5Cpp.h>
#include <pugixml.hpp>
#include <armadillo>

namespace YAML {
    template<>
    struct convert<arma::vec> {
        static Node encode(const arma::vec& rhs);
        static bool decode(const Node& node, arma::vec& rhs);
    };
}

std::vector<double> readEloss(const std::string& path);
arma::Mat<uint16_t> readLUT(const std::string& path);
std::vector<std::vector<int>> parseXcfg(const std::string& path);

#endif /* def PARSERS_H */
