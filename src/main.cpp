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
#include <zlib.h>
#include <cppcodec/base64_rfc4648.hpp>
#include "zstr.hpp"
#include "nbt.hpp"
//using namespace Pistache;
using namespace cURLpp;
using namespace std;

std::string to_roman_numerals(const int &number) {
    switch (number) {
        case 1:
            return "I";
            break;
        case 2:
            return "II";
            break;
        case 3:
            return "III";
            break;
        case 4:
            return "IV";
            break;
        case 5:
            return "V";
            break;
        case 6:
            return "VI";
            break;
        case 7:
            return "VII";
        case 8:
            return "VIII";
            break;
        case 9:
            return "IX";
            break;
        case 10:
            return "X";
            break;
        default:
            return "";
    }
}
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


private:
    //Objects
    crow::SimpleApp Server;
    //Data
    mutex writing;
    atomic<int> threads = 0;
    multimap<string, pair<int, string>> GlobalPrices;
    int port = 8080;

    //Functions
    std::string to_roman_numerals(const int &number) {
        switch (number) {
            case 1:
                return "I";
                break;
            case 2:
                return "II";
                break;
            case 3:
                return "III";
                break;
            case 4:
                return "IV";
                break;
            case 5:
                return "V";
                break;
            case 6:
                return "VI";
                break;
            case 7:
                return "VII";
            case 8:
                return "VIII";
                break;
            case 9:
                return "IX";
                break;
            case 10:
                return "X";
                break;
            default:
                return "";
        }
    }

    int getPage(int page) {
        ++threads;
        ostringstream getStream;
        multimap<string, pair<int, string>> prices;
        getStream << options::Url("https://api.hypixel.net/skyblock/auctions?page=" + to_string(page));
        auto getJson = nlohmann::json::parse(getStream.str());
        int size = getJson["auctions"].size();
        if (getJson["success"].type() == nlohmann::detail::value_t::boolean && getJson["success"].get<bool>()) {
            for (int i = 0; i < size; ++i) {
                if (getJson["auctions"][i]["bin"].type() == nlohmann::detail::value_t::boolean) {
                    auto c = cppcodec::base64_rfc4648::decode(
                            getJson["auctions"][i]["item_bytes"].get<string>());
                    string d(c.begin(), c.end());

                    nbt::NBT nbtdata;
                    istringstream str(d);
                    zstr::istream decoded(str);
                    nbtdata.decode(decoded);
                    string ID;
                    if (nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                .at<nbt::TagString>("id") == "ENCHANTED_BOOK") {
                        auto enchantments = nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                .at<nbt::TagCompound>("enchantments");
                        ID = enchantments.base.begin()->first + "_" +
                             to_roman_numerals(enchantments.at<nbt::TagInt>(enchantments.base.begin()->first));
                        for (auto &f : ID) f = toupper(f);
                    } else if (nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                       .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                       .at<nbt::TagString>("id") == "PET") {
                        auto petInfo = nlohmann::json::parse(
                                nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                        .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                        .at<nbt::TagString>("petInfo"));
                        ID = petInfo["tier"].get<string>() + "_" + petInfo["type"].get<string>() + "_PET";

                    } else if (nbt::get_list<nbt::TagCompound>
                                       (nbtdata.at<nbt::TagList>("i"))[0]
                                       .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                       .at<nbt::TagString>("id").length() > 8
                               && nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                          .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                          .at<nbt::TagString>("id").substr(nbt::get_list<nbt::TagCompound>
                                                                                   (nbtdata.at<nbt::TagList>("i"))[0]
                                                                                   .at<nbt::TagCompound>(
                                                                                           "tag").at<nbt::TagCompound>(
                                            "ExtraAttributes")
                                                                                   .at<nbt::TagString>("id").length() -
                                                                           8) == "_STARRED") {
                        ID = nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                .at<nbt::TagString>("id").substr(0, nbt::get_list<nbt::TagCompound>(
                                        nbtdata.at<nbt::TagList>("i"))[0]
                                                                            .at<nbt::TagCompound>(
                                                                                    "tag").at<nbt::TagCompound>(
                                                "ExtraAttributes")
                                                                            .at<nbt::TagString>("id").length() - 8);
                    }
                    {
                        ID = nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                .at<nbt::TagString>("id");
                    }
                }
            }
        }
        --threads;
        return getJson["totalPages"];
    }

    void HyAPI() {
        GlobalPrices.clear();
    }

};

int main() {
    //    Pistache::Address addr(Pistache::Ipv4::any(), Pistache::Port(9080));
    initialize(CURL_GLOBAL_ALL);
//    ostringstream getStream;
//    multimap<string, pair<int, string>> prices;
//    getStream << options::Url("https://api.hypixel.net/skyblock/auctions?page=" + to_string(1));
//    auto getJson = nlohmann::json::parse(getStream.str());
//    int size = getJson["auctions"].size();
//    if (getJson["success"]) {
//        for (int i = 0; i < size; ++i) {
//            if (getJson["auctions"][i]["bin"].type() == nlohmann::detail::value_t::boolean) {
//                auto c = cppcodec::base64_rfc4648::decode(
//                        getJson["auctions"][i]["item_bytes"].get<string>());
//                string d(c.begin(), c.end());
//
//                nbt::NBT nbtdata;
//                istringstream str(d);
//                zstr::istream decoded(str);
//                nbtdata.decode(decoded);
//                cout<<nbtdata<<endl;
//            }
//        }
//    }
//    ostringstream getStream;
//    multimap<string, pair<int, string>> prices;
//    getStream << options::Url("https://api.hypixel.net/skyblock/auctions?page=" + to_string(0));
//    auto getJson = nlohmann::json::parse(getStream.str());
//    int size = getJson["auctions"].size();
//    if (getJson["success"].type() == nlohmann::detail::value_t::boolean && getJson["success"].get<bool>()) {
//        for (int i = 0; i < size; ++i) {
//            if (getJson["auctions"][i]["bin"].type() == nlohmann::detail::value_t::boolean) {
//                auto c = cppcodec::base64_rfc4648::decode(
//                        getJson["auctions"][i]["item_bytes"].get<string>());
//                string d(c.begin(), c.end());
//
//                nbt::NBT nbtdata;
//                istringstream str(d);
//                zstr::istream decoded(str);
//                nbtdata.decode(decoded);
//                if (nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
//                                   .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
//                                   .at<nbt::TagString>("id") == "PET") {
//                    auto petInfo = nlohmann::json::parse(
//                            nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
//                            .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
//                            .at<nbt::TagString>("petInfo"));
//                    string f = petInfo["tier"].get<string>() + "_" + petInfo["type"].get<string>() + "_PET";
//                    cout<<nbtdata<<endl;
//
//                } else if(nbt::get_list<nbt::TagCompound>
//                (nbtdata.at<nbt::TagList>("i"))[0]
//                .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
//                .at<nbt::TagString>("id").length() > 8
//                && nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
//                .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
//                .at<nbt::TagString>("id").substr(nbt::get_list<nbt::TagCompound>
//                        (nbtdata.at<nbt::TagList>("i"))[0]
//                .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
//                .at<nbt::TagString>("id").length()-8)=="_STARRED")
//                {
//                    cout<<"STARRED"<<endl;
//                }//else
////                    cout << nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
////                            .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
////                            .at<nbt::TagString>("id") << endl;
//            }
//        }
//    }
//    for(int t=0; t<d.size(); ++t)
//    {
//        cout<<int(d[t])<<endl;
//    }
//     NBT::NBTReader::LoadFromData((unsigned char *)(void*)(d.c_str()), d.size());
//    enkiNBTDataStream stream;
//    enkiNBTInitFromMemoryCompressed(&stream, (uint8_t *) d.c_str(), d.size(), 0);
//    ostringstream getStream;
//    getStream << options::Url("https://api.hypixel.net/skyblock/auctions?page=" + to_string(0));
//    auto getJson = nlohmann::json::parse(getStream.str());
//    int size = getJson["auctions"].size();
//    if (getJson["success"]) {
//        for (int i = 0; i < size; ++i) {
//
//        }
//    }
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


