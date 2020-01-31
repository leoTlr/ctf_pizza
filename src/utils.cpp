#include "utils.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>

// /target?query=foo -> /target
boost::string_view targetWithoutQueryParams(boost::string_view target) {
    size_t pos = 0;
    if ((pos = target.find('?')) == boost::string_view::npos)
        return target.substr(0);
    else if (pos - 1 < 1) // case if /?foo=bar
        return target.substr(0, 1);
    return target.substr(0, pos);
}

// parse request target for queries (i.e. /target?foo=bar&somequery=stuff)
struct query_params parseTargetQuery(boost::string_view target) {
    size_t query_pos = 0, separator_pos = 0;
    struct query_params params {-1, false};

    if ((query_pos = target.find('?')) == boost::string_view::npos) {
        params.id = -1;
        return params; // no '?' in target -> no queries to parse
    }

    // loop through all queries splitting keys and values and adding values to return struct
    do  {
        separator_pos = target.find('=', query_pos);
        boost::string_view query = target.substr(query_pos + 1, separator_pos - query_pos - 1);

        if (beast::iequals(query, "order_id")) {

            size_t len_str_val = target.find('&', separator_pos) - separator_pos;
            const auto val = target.substr(separator_pos + 1, len_str_val);

            int id = 0;
            try {
                id = std::stoi(val.to_string());
            } catch (...) {
                params.id = -2;
                continue; // str->int conversion failed
            }
            // val < 0 -> invalid val
            params.id = id >= 0 ? id : -2;
        } 
        else if (beast::iequals(query, "debug")) {
            size_t len_str_val = target.find('&', separator_pos) - separator_pos;
            const auto val = target.substr(separator_pos + 1, len_str_val);

            if (beast::iequals(val, "true"))
                params.debug = true;
        }
    } while ((query_pos = target.find('&', query_pos + 1)) != boost::string_view::npos);    

    return params;
}

char from_hex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

// 12345+Stadt+Stra%C3%9Fe+1
// -> 12345 Stadt Straße 1
std::string url_decode(std::string const& text) {
    char h;
    std::ostringstream escaped;
    escaped.fill('0');

    for (auto i = text.begin(), n = text.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        if (c == '%') {
            if (i[1] && i[2]) {
                h = from_hex(i[1]) << 4 | from_hex(i[2]);
                escaped << h;
                i += 2;
            }
        } else if (c == '+') {
            escaped << ' ';
        } else {
            escaped << c;
        }
    }
    return escaped.str();
}

// extract order data from urlencoded form data (http body)
struct order_form_data extractFormData(std::string const& body) {
    std::stringstream body_data (url_decode(body));
    std::string token, key, val;
    std::size_t separator_pos = 0, id = 0;

    struct order_form_data order;

    while (getline(body_data, token, '&')) {

        separator_pos = token.find('=');
        if (separator_pos == std::string::npos || separator_pos == 0 || separator_pos > token.size() - 1)
            continue;

        key = token.substr(0, separator_pos);
        val = token.substr(separator_pos + 1, token.size()+1);
        if (key == "pizza_id") {
            try {
                id = std::stoul(val);
            } catch (...) {
                continue; // str->int conversion failed
            }
            order.pizza_ids.push_back(id);
        } 
        else if (key == "name")
            order.name = val;
        else if (key == "address")
            order.address = val;
    }

    return order;
}

// write order data into db
std::optional<int> place_order(std::string const& dbpath, struct order_form_data const& data) {

    sqlite::database db = sqlite::database(dbpath);
    int order_id = 0;
    try {
        db  << "begin;";

        std::string timestamp;
        db  << "select datetime('now');" 
            >> timestamp;

        db  << "insert into 'order' values (NULL,?,?,?);"
            << data.address
            << data.name
            << timestamp;
        
        order_id = db.last_insert_rowid();
        auto statement = db  << "insert into 'order_pizza' (order_id, pizza_id) values (?,?);";
        for (auto pizza_id : data.pizza_ids) {
            statement   << order_id 
                        << pizza_id;
            statement++; // reset bound data
        }

        db << "commit;";
    }
    catch (sqlite::sqlite_exception& e) {
        std::cerr  << "[ERROR] place_order(): "
                << e.what() << " during: '" << e.get_sql() << "'"
                << std::endl;
        return std::nullopt;
    }
    
    return order_id;
}

// fetch order data to order_id from db
std::optional<struct receipt_data> get_receipt_info(std::string const& dbpath, const int order_id) {

    sqlite::database db = sqlite::database(dbpath);
    struct receipt_data data = {};
    struct receipt_line line = {};
    std::unordered_map<int,receipt_line> count_map;

    try {
        db  <<  "begin;";
        db  <<  "select address, name, timestamp from 'order' where order_id = ?;"
            <<  order_id
            >>  [&](std::string address, std::string name, std::string timestamp) {
                    // strings will be empty on empty result set
                    data.address = address;
                    data.name = name;
                    data.timestamp = timestamp;
                };

        if (data.timestamp.empty()) {
            db << "rollback;";
            return std::nullopt;
        }

        db  <<  "select p.pizza_id, p.price, p.description "
                "from order_pizza as op "
                "inner join pizza as p on p.pizza_id = op.pizza_id "
                "where op.order_id = ?; " 
            <<  order_id
            >>  [&](int pizza_id, float price, std::string description) {
                    // query may contain multiple entries of same pizza
                    // -> count ulitizing a hashmap on pizza_id<->receipt_line
                    if (count_map.find(pizza_id) != count_map.end()) 
                        count_map[pizza_id].count++;
                    else
                        count_map[pizza_id] = {pizza_id, description, 1, price};
                };
        db  << "commit;";
    }
    catch (sqlite::sqlite_exception& e) {
        std::cerr  << "[ERROR] get_receipt_info(): "
                << e.what() << " during: '" << e.get_sql() << "'"
                << std::endl;
        return std::nullopt;
    }

    data.lines.reserve(count_map.size());
    for (auto const& kv : count_map)
        data.lines.push_back(kv.second);

    return data;
}

// returns all pizza_id's in table pizza
std::optional<std::unordered_set<int>> get_pizza_ids(std::string const& dbpath) {

    sqlite::database db = sqlite::database(dbpath);
    std::unordered_set<int> ids; 

    try {
        db  <<  "begin;";
        db  <<  "select pizza_id from pizza;"
            >>  [&](int pizza_id) {
                    ids.insert(pizza_id);
                };
        db  << "commit;";
    }
    catch (sqlite::sqlite_exception& e) {
        std::cerr  << "[ERROR] get_pizza_ids(): "
                << e.what() << " during: '" << e.get_sql() << "'"
                << std::endl;
        return std::nullopt;
    }

    return ids;
}
