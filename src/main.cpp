#define CROW_MAIN

#include <iostream>
#include <atomic>
#include <string>
//#include <pistache/common.h>
//#include <pistache/cookie.h>
//#include <pistache/endpoint.h>
//#include <pistache/http.h>
//#include <pistache/net.h>
//#include <pistache/http_headers.h>
//#include <pistache/peer.h>
//#include <pistache/router.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <nlohmann/json.hpp>
#include <crow_all.h>
//using namespace Pistache;
using namespace cURLpp;
using namespace std;

struct Server {
public:
    //Functions
    void initialize() {
        CROW_ROUTE(Server, "/")([] {
            return "Healthy";
        });
    }

    void start() {
        Server.port(port).multithreaded().run();
    }

    void HyAPI() {

    }

    void getPage(int page) {
        ++threads;
        ostringstream getStream;
        map<string, vector<pair<string, int>>> prices;
        getStream << options::Url("https://api.hypixel.net/skyblock/auctions" + to_string(page));
        auto getJson = nlohmann::json::parse(getStream.str());
        int size = getJson["auctions"].size();
        if (getJson["success"]) {
            for (int i = 0; i < size; ++i) {
                if (getJson["auctions"][i]["bin"])
                    cout << "BIN\n";
            }
        }
        --threads;
        return;
    }

private:
    //Objects
    crow::SimpleApp Server;
    //Data
    mutex writing;
    atomic<int> threads = 0;

    int port = 8080;
    //Functions
};

int main() {
    //    Pistache::Address addr(Pistache::Ipv4::any(), Pistache::Port(9080));
    initialize(CURL_GLOBAL_ALL);
    ostringstream getStream;
    getStream << options::Url("https://api.hypixel.net/skyblock/auctions?page=" + to_string(0));
    auto getJson = nlohmann::json::parse(getStream.str());
    int size = getJson["auctions"].size();
    if (getJson["success"]) {
        for (int i = 0; i < size; ++i) {
            if (getJson["auctions"][i]["bin"].type() == nlohmann::detail::value_t::boolean)
                cout << "BIN\n";
            else cout << "NOT BIN\n";
        }
    }
//    Server HyAuctions;
//    HyAuctions.initialize();
//    HyAuctions.start();
    //    HttpHandler handler(addr);
    //    handler.init(4);
    //    handler.start();
    cURLpp::terminate();
    return 0;
}
// LEGACY CODEBASE
//class HttpHandler
//        {
//        public:
//            explicit HttpHandler(Address addr)
//            : httpEndpoint(std::make_shared<Http::Endpoint>(addr))
//            {}
//
//            void init(size_t thr = 2)
//            {
//                auto opts = Http::Endpoint::options()
//                        .threads(static_cast<int>(thr));
//                httpEndpoint->init(opts);
//                setupRoutes();
//            }
//
//            void start()
//            {
//                running=true;
//                auto HyClientThread = std::make_unique<std::thread>([this] { HypixelFetch(); });
//                httpEndpoint->setHandler(router.handler());
//                httpEndpoint->serve();
//            }
//        private:
//            std::shared_ptr<Http::Endpoint> httpEndpoint;
//            Rest::Router router;
////            Http::Experimental::Client hypixel;
//            const string AuctionEndpoint = "https://api.hypixel.net/skyblock/auctions";
//            atomic<bool> running = false;
//            std::string sniper, bin_full, bin_free;
//            cURLpp::Easy HyHandle;
//
//
//            void setupRoutes()
//            {
//                using namespace Rest;
//
//                Routes::Get(router, "/ready", Routes::bind(&HttpHandler::isUp, this));
//
//            }
//            void isUp(const Rest::Request&, Http::ResponseWriter response)
//            {
//                response.send(Http::Code::Ok, "1");
//            }
//            void HypixelFetch()
//            {
//
//                HyHandle.setOpt(cURLpp::Options::Url(AuctionEndpoint+"?page=0"));
//                HyHandle.perform();
//
////                auto HyOpts = Http::Experimental::Client::options().threads(1).maxConnectionsPerHost(8);
////                hypixel.init(HyOpts);
////                int timestamp=0;
////                while(running){
////                    int pages=0;
////                    std::vector<Async::Promise<Http::Response>> responses;
////                    auto response = hypixel.get(AuctionEndpoint).send();
////                    response.then(
////                            [&](Http::Response response1)
////                            {
////                                cout<<response1.code()<<endl<<response1.body()<<endl;
////                                },
////                                [&](std::exception_ptr exc) {
////                                PrintException excPrinter;
////                                excPrinter(exc);
////                            }
////                            );
////                    break;
////                }
////                return;
//            }
//        };


