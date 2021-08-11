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
#define cURL::CURLOPT_TCP_NO_DELAY

#include <curlpp/Infos.hpp>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "json.hpp"
#include "crow_all.h"
#include <zlib.h>
#include <cppcodec/base64_rfc4648.hpp>
#include "zstr.hpp"
#include "nbt.hpp"
#include <sys/wait.h>
#include <sys/types.h>
#include <fstream>
//using namespace Pistache;
using namespace cURLpp;
using namespace std;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;


class Server {
public:
    //Functions
    void initialize() {
        CROW_ROUTE(CrowServer, "/")([] {
            return "Healthy";
        });
        CROW_ROUTE(CrowServer, "/sniper")([this](const crow::request &req) {
            if (req.url_params.get("uuid") != nullptr && req.url_params.get("key") != nullptr &&
                authenticated(string(req.url_params.get("uuid")),
                              string(req.url_params.get("key")),
                              req.remoteIpAddress)) {
                auto response = crow::response(200, nlohmann::json::object({{"success",  true},
                                                                            {"auctions", sniper}}).dump());
                response.add_header("Content-Type", "application/json");
                return response;
            } else {
                return crow::response(403);
            }
        });
        CROW_ROUTE(CrowServer, "/bin_full")([this](const crow::request &req) {
            if (req.url_params.get("uuid") != nullptr && req.url_params.get("key") != nullptr &&
                authenticated(string(req.url_params.get("uuid")),
                              string(req.url_params.get("key")),
                              req.remoteIpAddress)) {
                auto response = crow::response(200, nlohmann::json::object({{"success",  true},
                                                                            {"auctions", bin_full}}).dump());
                response.add_header("Content-Type", "application/json");
                return response;
            } else {
                return crow::response(403);
            }
        });
        CROW_ROUTE(CrowServer, "/bin_free")([this] {
            auto response = crow::response(200, nlohmann::json::object({{"success",  true},
                                                                        {"auctions", bin_free}}).dump());
            response.add_header("Content-Type", "application/json");
            return response;
        });
        CROW_ROUTE(CrowServer, "/misc")([this](const crow::request &req) {
            if (req.url_params.get("uuid") != nullptr && req.url_params.get("key") != nullptr &&
                authenticated(string(req.url_params.get("uuid")),
                              string(req.url_params.get("key")),
                              req.remoteIpAddress)) {
                auto response = crow::response(200, nlohmann::json::object({{"success",  true},
                                                                            {"auctions", unsortable}}).dump());
                response.add_header("Content-Type", "application/json");
                return response;
            } else {
                return crow::response(403);
            }

        });
    }

    void start() {
        running = true;
        getThreaded.resize(200);
        for (int page = 0; page < 200; ++page) {
            getThreaded[page].setOpt(options::Url("https://api.hypixel.net/skyblock/auctions?page=" + to_string(page)));
            getThreaded[page].setOpt(curlpp::options::WriteStream(&getStream[page]));
            getThreaded[page].setOpt(options::TcpNoDelay(true));
        }
//        thread AvgApi([this]() { get3DayAvg(); });
        thread HyApi([this]() { HyAPI(); });
        thread AuthApi([this]() { getAuth(); });
        thread ClearCache([this]() { clearCache(); });
        CrowServer.port(port).multithreaded().run();
        std::lock_guard<std::mutex> clock(exit);
        running = false;
        exitc.notify_all();
        AuthApi.join();
        cout << "AuthAPI exited." << endl;
//        AvgApi.join();
        cout << "AvgAPI exited." << endl;
        HyApi.join();
        cout << "HyAPI exited." << endl;

        //Server.port(port).multithreaded().run();
    }


private:
    //Objects
    crow::SimpleApp CrowServer;
    //Data
    mutex writing, lock, exit;
    condition_variable exitc;
    atomic<int> threads = 0;
    vector<curlpp::Easy> getThreaded;
    unordered_map<string, set<tuple<int, string, long long, string, string>>> GlobalPrices;
    unordered_map<string, pair<string, tuple<int, string, long long, string, string>>> Cache;
    set<string> UniqueIDs;
    atomic<bool> running;
    nlohmann::json AveragePrice;
    nlohmann::json sniper, bin_full,
            bin_free, unsortable;
    atomic<long long> updated = 0;
    unordered_map<string, string> Auth;
    ostringstream getStream[200];
    int port = 80;
    const int margin = 1000000;
    map<string, int> tiers{
            {"COMMON",    0},
            {"UNCOMMON",  1},
            {"RARE",      2},
            {"EPIC",      3},
            {"LEGENDARY", 4},
            {"MYTHIC",    5}
    };

    //Functions
    static bool const isLevel100(int tier, int xp) {
        switch (tier) {
            case 0:
                return xp >= 5624785;
            case 1:
                return xp >= 8644220;
            case 2:
                return xp >= 12626665;
            case 3:
                return xp >= 17222200;
            default:
                return xp >= 25353230;
        }
    }

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

    int const to_tier(string tier) {
        return tiers[tier];
    }

    void get3DayAvg() {
        while (running) {
            lock.lock();
            ostringstream getStream;
            getStream << options::Url("https://moulberry.codes/auction_averages/3day.json.gz");
            istringstream str(getStream.str());
            zstr::istream decoded(str);
            decoded >> AveragePrice;
            lock.unlock();
            for (int i = 0; i < 720; ++i) {
                if (!running) return; else this_thread::sleep_for(chrono::seconds(15));
            }
        }
    }

    void clearCache() {
        while (running) {
            if (Cache.size() > 10000000) {
                Cache.clear();
            }
            this_thread::sleep_for(chrono::seconds(60));
        }
    }

    void getAuth() {
        while (running) {
            ifstream WhitelistJson("whitelist.json");
            auto whitelist = nlohmann::json::parse(WhitelistJson);
            Auth = whitelist.get<unordered_map<string, string >>();
            this_thread::sleep_for(chrono::seconds(15));
        }
    }

    pair<int, long long> getPage(int page) {
        ++threads;
        auto starttime = high_resolution_clock::now();
        unordered_map<string, set<tuple<int, string, long long, string, string>>> prices;
        unordered_map<string, pair<string, tuple<int, string, long long, string, string>>> local_cache;
        set<string> localUniqueIDs;
        getThreaded[page].perform();
        auto http_code = curlpp::infos::ResponseCode::get(getThreaded[page]);
        if (http_code != 200 || getStream[page].str() == "") return {0, updated};
        auto downloadtime = high_resolution_clock::now();
        auto getJson = nlohmann::json::parse(getStream[page].str());
        int size = getJson["auctions"].is_null() ? 0 : getJson["auctions"].size();
        if (getJson["success"].type() == nlohmann::detail::value_t::boolean && getJson["success"].get<bool>()) {
            for (int i = 0; i < size; ++i) {
//                cout<<"Analysing "<<i<<" on page "<<page<<'\n';
                if (getJson["auctions"][i]["bin"].type() == nlohmann::detail::value_t::boolean) {
                    auto val = Cache.find(getJson["auctions"][i]["uuid"].get<string>());
                    if (val != Cache.end()) {
                        if (prices.find(val->second.first) != prices.end()) {
                            prices.find(val->second.first)->
                                    second.insert(val->second.second);
                        } else {
                            prices.insert({val->second.first, {val->second.second}});
                            localUniqueIDs.insert(val->second.first);
                        }
                    } else {
                        auto c = cppcodec::base64_rfc4648::decode(
                                getJson["auctions"][i]["item_bytes"].get<string>());
                        string d(c.begin(), c.end());

                        nbt::NBT nbtdata;
                        istringstream str(d);
                        zstr::istream decoded(str);
                        nbtdata.decode(decoded);
                        string ID;
                        if (nbt::get_list<nbt::TagCompound>
                                    (nbtdata.at<nbt::TagList>("i"))[0]
                                    .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes").base
                                    .find("enchantments") != nbt::get_list<nbt::TagCompound>
                                    (nbtdata.at<nbt::TagList>("i"))[0]
                                    .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes").base.end() &&
                            nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                    .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                    .at<nbt::TagString>("id") == "ENCHANTED_BOOK") {
                            auto enchantments = nbt::get_list<nbt::TagCompound>
                                    (nbtdata.at<nbt::TagList>("i"))[0]
                                    .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                    .at<nbt::TagCompound>("enchantments");
                            ID = enchantments.base.begin()->first + ";" +
                                 to_string(enchantments.at<nbt::TagInt>(enchantments.base.begin()->first));
                            for (auto &f : ID) f = toupper(f);
                        } else if (nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                           .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                           .at<nbt::TagString>("id") == "PET") {
                            auto petInfo = nlohmann::json::parse(
                                    nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                            .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                            .at<nbt::TagString>("petInfo"));
                            ID = petInfo["type"].get<string>() + ";" + to_string(to_tier
                                                                                         (petInfo["tier"].get<string>()))
                                 + (isLevel100(to_tier(petInfo["tier"]),
                                               petInfo["exp"]) ? ";100" : "");

                        } else if (nbt::get_list<nbt::TagCompound>
                                           (nbtdata.at<nbt::TagList>("i"))[0]
                                           .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                           .at<nbt::TagString>("id").length() > 8
                                   && nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                              .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                              .at<nbt::TagString>("id").substr(nbt::get_list<nbt::TagCompound>
                                                                                       (nbtdata.at<nbt::TagList>(
                                                                                               "i"))[0]
                                                                                       .at<nbt::TagCompound>(
                                                                                               "tag").at<nbt::TagCompound>(
                                                "ExtraAttributes")
                                                                                       .at<nbt::TagString>(
                                                                                               "id").length() -
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
                        if (prices.find(ID) != prices.end()) {
                            prices.find(ID)->second.insert(tuple<int, string, long long, string, string>(
                                    getJson["auctions"][i]["starting_bid"],
                                    getJson["auctions"][i]["uuid"],
                                    getJson["auctions"][i]["start"],
                                    getJson["auctions"][i]["item_name"],
                                    getJson["auctions"][i]["tier"]));
                        } else {
                            prices.insert({ID, {tuple<int, string, long long, string, string>(
                                    getJson["auctions"][i]["starting_bid"],
                                    getJson["auctions"][i]["uuid"],
                                    getJson["auctions"][i]["start"],
                                    getJson["auctions"][i]["item_name"],
                                    getJson["auctions"][i]["tier"])}});
                            localUniqueIDs.insert(ID);
                        }
                        local_cache.insert({getJson["auctions"][i]["uuid"], {
                                ID, tuple<int, string, long long, string, string>(
                                        getJson["auctions"][i]["starting_bid"],
                                        getJson["auctions"][i]["uuid"],
                                        getJson["auctions"][i]["start"],
                                        getJson["auctions"][i]["item_name"],
                                        getJson["auctions"][i]["tier"])}});
                    }
                }

            }
            writing.lock();
            UniqueIDs.merge(localUniqueIDs);
            GlobalPrices.merge(prices);
            for (auto &i : prices) {
                GlobalPrices.find(i.first)->second.merge(i.second);
            }
            Cache.merge(local_cache);
            writing.unlock();
            auto endtime = high_resolution_clock::now();
            duration<double, std::milli> totalspeed = endtime - starttime;
            duration<double, std::milli> downloadspeed = downloadtime - starttime;
            cout << downloadspeed.count() << "ms, " << totalspeed.count() << "ms\n";
        }
        --threads;
        return pair<int, long long>(getJson["totalPages"], getJson["lastUpdated"]);
    }

    bool authenticated(string const &UUID, string const &key, string const &IP) {
        ofstream log;
        log.open("log.txt", ofstream::out | ofstream::app);
        auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        log << "Player UUID: " << UUID << " Key: " << key << " IP: " << IP <<
            " Time: " << std::put_time(std::localtime(
                &time), "%Y-%m-%d %X") << '\n';
        log.close();
        return (Auth.find(UUID) != Auth.end() && Auth.find(UUID)->second == key);
    }

    void HyAPI() {
        while (running) {
            lock.lock();
            auto starttime = high_resolution_clock::now();
            UniqueIDs.clear();
            GlobalPrices.clear();
            vector<std::thread> children;
            cout << "Beginning Cycle\n";
            pair<int, long long> information = getPage(0);
            children.reserve(information.first);
            if (information.second == updated) {
                cout << "HyAPI has not updated yet. Sleeping Thread.\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                lock.unlock();
                continue;
            } else {
                sniper.clear();
                bin_full.clear();
                bin_free.clear();
                unsortable.clear();
                updated = information.second;
                long long lastUpdate = information.second - 60000;
                for (int i = 1; i < information.first; ++i) {
//                      getPage(i);
                    children.emplace_back([this](int i) { getPage(i); }, i);
                }
                for (auto &child : children)
                    child.join();//This sleeps until all fetching threads are done.
                //Processing
                auto downloadtime = high_resolution_clock::now();
                for (auto &member : UniqueIDs) {
                    if (GlobalPrices.find(member)->second.size() >= 2) //2 or more
                    {
                        if (AveragePrice[member]["price"].is_null()) {
                            //cout<<"COULD NOT FIND AVERAGE PRICE FOR: "<<member<<"\n";
                            if (get<0>(*(GlobalPrices.find(member)->second.begin())) <=
                                get<0>(*next(GlobalPrices.find(member)->second.begin())) - margin) {
                                if (get<2>(*GlobalPrices.find(member)->second.begin()) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*GlobalPrices.find(member)->second.begin())),
                                            make_pair("sell_price",
                                                      get<0>(*next(GlobalPrices.find(member)->second.begin())) - 1),
                                            make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*GlobalPrices.find(member)->second.begin())),
                                            make_pair("sell_price",
                                                      get<0>(*next(GlobalPrices.find(member)->second.begin())) - 1),
                                            make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                                }
                            } else if (get<0>(*GlobalPrices.find(member)->second.begin()) <=
                                       get<0>(*next(GlobalPrices.find(member)->second.begin()))) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                        make_pair("item_name", (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                        make_pair("buy_price", get<0>(*GlobalPrices.find(member)->second.begin())),
                                        make_pair("sell_price",
                                                  get<0>(*next(GlobalPrices.find(member)->second.begin())) - 1),
                                        make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                            }
                        } else {
                            if (get<0>(*GlobalPrices.find(member)->second.begin()) <=
                                min(get<0>(*next(GlobalPrices.find(member)->second.begin())),
                                    AveragePrice[member]["price"].get<int>()) - margin) {
                                if (get<2>(*GlobalPrices.find(member)->second.begin()) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*GlobalPrices.find(member)->second.begin())),
                                            make_pair("sell_price",
                                                      min(AveragePrice[member]["price"].get<int>(),
                                                          get<0>(*next(GlobalPrices.find(member)->second.begin()))) -
                                                      1),
                                            make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*GlobalPrices.find(member)->second.begin())),
                                            make_pair("sell_price",
                                                      min(AveragePrice[member]["price"].get<int>(),
                                                          get<0>(*next(GlobalPrices.find(member)->second.begin()))) -
                                                      1),
                                            make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                                }
                            } else if (get<0>(*GlobalPrices.find(member)->second.begin()) <
                                       min(get<0>(*next(GlobalPrices.find(member)->second.begin())),
                                           AveragePrice[member]["price"].get<int>())) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                        make_pair("item_name", (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                        make_pair("buy_price", get<0>(*GlobalPrices.find(member)->second.begin())),
                                        make_pair("sell_price",
                                                  min(AveragePrice[member]["price"].get<int>(),
                                                      get<0>(*next(GlobalPrices.find(member)->second.begin()))) - 1),
                                        make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                            }
                        }
                    } else {
                        if (AveragePrice[member].is_null()) {
                            // cout<<"UNABLE TO CLASSIFY: "<<member<<"\n";
                            unsortable[unsortable.size()] = {
                                    make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                    make_pair("item_name", (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                    make_pair("buy_price", get<0>(*GlobalPrices.find(member)->second.begin())),
                                    make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};

                        } else {
                            if (get<0>(*GlobalPrices.find(member)->second.begin()) <=
                                AveragePrice[member]["price"].get<int>() - margin) {
                                if (get<2>(*GlobalPrices.find(member)->second.begin()) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*GlobalPrices.find(member)->second.begin())),
                                            make_pair("sell_price",
                                                      get<0>(*next(GlobalPrices.find(member)->second.begin())) - 1),
                                            make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*GlobalPrices.find(member)->second.begin())),
                                            make_pair("sell_price",
                                                      get<0>(*next(GlobalPrices.find(member)->second.begin())) - 1),
                                            make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                                }
                            } else if (get<0>(*GlobalPrices.find(member)->second.begin()) <
                                       AveragePrice[member]["price"].get<int>()) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(*GlobalPrices.find(member)->second.begin()))),
                                        make_pair("item_name",
                                                  (get<3>(*GlobalPrices.find(member)->second.begin()))),
                                        make_pair("buy_price", get<0>(*GlobalPrices.find(member)->second.begin())),
                                        make_pair("sell_price",
                                                  get<0>(*next(GlobalPrices.find(member)->second.begin())) - 1),
                                        make_pair("tier", (get<4>(*GlobalPrices.find(member)->second.begin())))};
                            }
                        }
                    }
                }
                auto endtime = high_resolution_clock::now();
                duration<double, std::milli> totalspeed = endtime - starttime;
                duration<double, std::milli> processingspeed = endtime - downloadtime;
                cout << "Processing speed: " << processingspeed.count()
                     << "ms, Total speed: " << totalspeed.count() << "ms" << endl;
            }
            lock.unlock();
            this_thread::sleep_until(
                    std::chrono::system_clock::time_point(std::chrono::milliseconds{updated + 62000}));
        }
    }

};

int main() {
    //    Pistache::Address addr(Pistache::Ipv4::any(), Pistache::Port(9080));
    initialize();
    Server entrypoint;
    entrypoint.initialize();
    entrypoint.start();
    cURLpp::terminate();
    return 0;
}
