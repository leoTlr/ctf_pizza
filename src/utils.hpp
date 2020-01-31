#ifndef UTILS_HPP
#define UTILS_HPP

#include <boost/beast.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include "../include/sqlite_modern_cpp/hdr/sqlite_modern_cpp.h"


struct query_params {
    int id = 0;
    bool debug = false;
};

struct order_form_data {
    std::string name;
    std::string address;
    std::vector<int> pizza_ids;
};

struct receipt_line {
    int pizza_id = 0;
    std::string description;
    int count = 0;
    float price = 0.0;
};

struct receipt_data {
    std::string name;
    std::string address;
    std::string timestamp;
    std::vector<struct receipt_line> lines;
};

boost::string_view targetWithoutQueryParams(boost::string_view target);

struct query_params parseTargetQuery(boost::string_view target);

std::string url_decode(std::string const& text);

struct order_form_data extractFormData(std::string const& body);

std::optional<int> place_order(std::string const& dbpath, struct order_form_data const& data);

std::optional<struct receipt_data> get_receipt_info(std::string const& dbpath, const int order_id);

std::optional<std::unordered_set<int>> get_pizza_ids(std::string const& dbpath);

#endif // UTILS_HPP