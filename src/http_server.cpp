#include <boost/beast/core/detail/base64.hpp> // caution: could move somwhere else
#include <boost/optional.hpp>
#include <exception> // std::out_of_range for http::basic_fields::at
#include <fstream>
#include <unordered_set>

#include "http_server.hpp"
#include "utils.hpp"
#include "../include/cpp-jwt/jwt/jwt.hpp"
#include "../include/sqlite_modern_cpp/hdr/sqlite_modern_cpp.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace fs = std::filesystem;

// asynchonously recieve complete request message
void HttpConnection::readRequest() {
    auto self = shared_from_this();

    http::async_read(
        socket_,
        readbuf_,
        request_,
        [self](beast::error_code ec, std::size_t bytes_transferred) {

            boost::ignore_unused(bytes_transferred);
            if (!ec)
                self->processRequest();
            else {
                if (ec.message().substr(0, 3) == "bad") // bad [ target|version|... ]
                    self->writeResponse(self->BadRequest(ec.message()));
                else {
                    fail(ec, "readRequest()");
                    self->writeResponse(self->ServerError(ec.message()));
                }
            }
        });
}

void HttpConnection::processRequest() {

    switch (request_.method()) {
        case http::verb::get:
            return handleGET();
        case http::verb::post:
            return handlePOST();
        default:
            return writeResponse(BadRequest("invalid request method"));
    }
}

void HttpConnection::handleGET() {

    std::cout << "GET " << request_.target() << std::endl;
    
    if (request_.target().empty() || request_.target()[0] != '/')
        return writeResponse(BadRequest("invalid request target"));

    boost::string_view target = targetWithoutQueryParams(request_.target());

    // check for specific targets
    if (target == "/pubkey")
        return writeResponse(PubKeyResponse());
    else if (target == "/receipt") {
        const struct query_params query = parseTargetQuery(request_.target());
        if (query.id < 0)
            return writeResponse(BadRequest("no or invalid order_id provided"));

        auto token = extractJWT();
        if (!token.has_value())
            return writeResponse(Unauthorized("missing or malformed token"));
        if (!verifyJWT(token.value().to_string(), query.id) && !query.debug) {
            return writeResponse(Unauthorized("invalid token provided"));
        } else {
            auto const data = get_receipt_info(dbpath_, query.id);
            if (!data.has_value())
                return writeResponse(BadRequest("order_id not found"));
            return writeResponse(ReceiptDataRasponse(data.value()));
        }
    } 

    // handle static files
    auto const srvdir = fs::path("static");
    auto const requested_file = (target == "/") ? fs::path("index.html") : fs::path(target.substr(1).to_string());
    auto const path = srvdir / requested_file;
    if (fs::exists(path))
        return writeResponse(StaticResponse(path));
    return writeResponse(NotFound(requested_file.c_str()));
}

void HttpConnection::handlePOST() {

    std::cout << "POST " << request_.target() << std::endl;

    if (request_.body().size() < 1)
        return writeResponse(BadRequest("form data missing"));    

    auto const target = targetWithoutQueryParams(request_.target()).to_string();
    if (!(target == "/order"))
        return writeResponse(NotFound(target));
    try {
        if (!request_.at(http::field::content_type).starts_with("application/x-www-form-urlencoded"))
            return writeResponse(BadRequest("content type has to be 'application/x-www-form-urlencoded'"));
    } catch (...) {
        return writeResponse(BadRequest("content type has to be 'application/x-www-form-urlencoded'"));
    }

    struct order_form_data order = extractFormData(request_.body());
    auto valid_ids = get_pizza_ids(dbpath_);

    if (!valid_ids.has_value())
        return writeResponse(ServerError("error processing order"));
    else if (order.address.empty() || order.name.empty() || order.pizza_ids.empty())
        return writeResponse(BadRequest("insufficient form data"));
    else if (std::all_of(order.pizza_ids.begin(), order.pizza_ids.end(), [&valid_ids](int id){return valid_ids.value().count(id) != 1;}))
        return writeResponse(BadRequest("bad pizza_id"));
        
    auto order_id = place_order(dbpath_, order);

    if (!order_id.has_value())
        return writeResponse(ServerError("failed to place order"));    

    return writeResponse(TokenResponse(newToken(order_id.value())));
}

// close conn after wait
void HttpConnection::checkDeadline() {
    auto self = shared_from_this();

    deadline_.async_wait(
        [self] (beast::error_code ec) {
            if (!ec)
                self->socket_.close(ec);
        });
}

// get JWT string out of request header
std::optional<boost::string_view> HttpConnection::extractJWT() const {

    try {
        boost::string_view token_encoded = request_.at(http::field::authorization);

        if (beast::iequals(token_encoded.substr(0,6), "Bearer")) {
            token_encoded.remove_prefix(7);
            return token_encoded;
        } else return std::nullopt; // wrong authentification method

    } catch (std::out_of_range&) { // no Authorization header field
        return std::nullopt;
    }
}

std::string HttpConnection::newToken(const int order_id) const {

    auto new_token = jwt::jwt_object {
        jwt::params::algorithm("RS256"),
        jwt::params::secret(priv_key_)
    };

    new_token.add_claim("iss", server_name_);
    new_token.add_claim("aud", std::to_string(order_id));

    return new_token.signature();
}

// take token string (encoded) and verify signature
// return true on success, false otherwise
bool HttpConnection::verifyJWT(std::string const& token, const int order_id) const {

    try {
        // get alg from token header
        auto header = jwt::jwt_header{token.substr(0, token.find('.'))};
        auto alg_used = jwt::alg_to_str(header.algo());
    
        std::error_code ec;
        auto decoded_token = jwt::decode(
            jwt::string_view(token), 
            jwt::params::algorithms({alg_used}), 
            ec,
            jwt::params::secret(pub_key_),
            jwt::params::issuer(server_name_),
            jwt::params::aud(std::to_string(order_id)), // request target has to match audience
            jwt::params::verify(true)
        );

        if (ec && ec != jwt::AlgorithmErrc::NoneAlgorithmUsed) {
            std::cout << "[WARNING] verifyJWT(): error during claim verification: " << ec.message() << std::endl;
            return false;
        }

    } catch (jwt::SignatureFormatError const& e) { // malformed sig
        std::cerr << "[WARNING] verifyJWT(): wrong signature format" << std::endl;
        return false;
    } catch (jwt::DecodeError const& e) { // malformed token
        std::cerr << "[WARNING] verifyJWT(): error decoding token" << std::endl;
        return false;
    } catch (jwt::VerificationError const& e) { // invalid sig
        return false;
    }
    
    return true;
}