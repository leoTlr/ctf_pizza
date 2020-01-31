#include <iostream>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sqlite3.h>

#include "http_server.hpp"
#include "../include/cpp-jwt/jwt/jwt.hpp"


using namespace std;

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace fs = std::filesystem;

// read key pair from provided paths into strings (required by jwt implementation)
// exits on error
// pair.first -> public key
// pair.second -> private_key
pair<string, string> read_rsa_keys(fs::path const& pub_key, fs::path const& priv_key) {

    if (!(pub_key.extension() == ".pem") || !(priv_key.extension() == ".pem")) {
        cerr << "please provide key files in .pem format" << endl;
        exit(EXIT_FAILURE);
    }

    pair<string, string> keypair;
    array<fs::path, 2> keyfile_paths {pub_key, priv_key};
    for (int i=0; i<2; i++) {

        auto path = keyfile_paths[i];
        std::ifstream keyfile (path);

        if (!keyfile) {
            cerr << "[ERROR] could not open \"" << path << "\"" << endl;
            exit(EXIT_FAILURE); 
        }

        const string pattern_private_key ("-----BEGIN RSA PRIVATE KEY-----");
        const string pattern_public_key ("-----BEGIN PUBLIC KEY-----");

        keyfile.seekg(0, std::ios::end);
        size_t fsize = keyfile.tellg();
        keyfile.seekg(0);

        string first_line;
        first_line.resize(pattern_private_key.size()+1);
        keyfile.getline(first_line.data(), first_line.size());

        if (first_line.find(pattern_public_key) == 0) {
            keyfile.seekg(0); // read from start again to get full file
            keypair.first.resize(fsize);
            keyfile.read(keypair.first.data(), fsize);
        }
        else if (first_line.find(pattern_private_key) == 0) {
            keyfile.seekg(0);
            keypair.second.resize(fsize);
            keyfile.read(keypair.second.data(), fsize);
        }
        else {
            cerr << "[ERROR] rsa key malformed: \"" << path << "\"" << endl;
            keyfile.close();
            exit(EXIT_FAILURE);
        }
        keyfile.close();

    }

    // test if jwt implementation actually takes the provided keys
    try {
        namespace jwtp = jwt::params;

        auto token = jwt::jwt_object {jwtp::algorithm("RS256"), jwtp::secret(keypair.second)};
        auto token_str = token.signature();

        auto decoded = jwt::decode(token_str, jwtp::algorithms({"RS256"}), jwtp::secret(keypair.first));

    } catch (jwt::SigningError const& e){
        cerr << "[ERROR] rsa priv key malformed: " << e.what() << endl;
        exit(EXIT_FAILURE);
    } catch (jwt::DecodeError const& e) {
        cerr << "[ERROR] rsa pub key malformed: " << e.what() << endl;
        exit(EXIT_FAILURE);
    }

    return keypair;
}

void usage(char** argv) {
    cerr << "usage: " << argv[0] << " <port> <dbfile> <pub_key.pem> <priv_key.pem>" << endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {

    if (argc != 5)
        usage(argv);

    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));

    net::io_context ioc {1};

    tcp::acceptor acc {ioc, {net::ip::make_address("0.0.0.0"), port}};
    tcp::socket sock {ioc};

    auto const dbpath = fs::path(argv[2]);
    auto const pub_key = fs::path(argv[3]);
    auto const priv_key = fs::path(argv[4]);

    if (!fs::exists(pub_key) || !fs::exists(priv_key)) {
        cerr << "could not find provided key files" << endl;
        exit(EXIT_FAILURE);
    } else if (!fs::exists(dbpath)) {
        cerr << "could not find provided db" << endl;
        exit(EXIT_FAILURE);
    }

    pair<string,string> keypair = read_rsa_keys(pub_key, priv_key);
    
    const string server_name = "pizzaservice v0.1";

    start_http_server(acc, sock, dbpath, keypair, server_name);

    // register SIGINT and SIGTERM handler
    net::signal_set signals {ioc, SIGINT, SIGTERM};
    signals.async_wait(
        [&] (beast::error_code const&, int) {
            std::cout << "\nsignal recieved. stopping server" << endl;
            ioc.stop();
        });

    std::cout << "started server on port " << port << endl;
    ioc.run();

    return EXIT_SUCCESS;
}