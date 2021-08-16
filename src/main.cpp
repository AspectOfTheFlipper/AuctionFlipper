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
        CROW_ROUTE(CrowServer, "/")([this]() {
            active = true;
            if (sleep) {
                return "Sleeping";
            }
            return "Healthy";
        });
        CROW_ROUTE(CrowServer, "/sniper")([this](const crow::request &req) {
            if (req.url_params.get("uuid") != nullptr && req.url_params.get("key") != nullptr &&
                authenticated(string(req.url_params.get("uuid")),
                              string(req.url_params.get("key")),
                              req.remoteIpAddress)) {
                active = true;
                nlohmann::json js1 = nlohmann::json::object();
                js1["success"] = true;
                js1["auctions"] = sniper;
                js1["updated"] = updated.load();
                auto response = crow::response(200, js1.dump());
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
                active = true;
                nlohmann::json js1 = nlohmann::json::object();
                js1["success"] = true;
                js1["auctions"] = bin_full;
                js1["updated"] = updated.load();
                auto response = crow::response(200, js1.dump());
                response.add_header("Content-Type", "application/json");
                return response;
            } else {
                return crow::response(403);
            }
        });
        CROW_ROUTE(CrowServer, "/bin_free")([this] {
            active = true;
            nlohmann::json js1 = nlohmann::json::object();
            js1["success"] = true;
            js1["auctions"] = bin_free;
            js1["updated"] = (long long) updated;
            auto response = crow::response(200, js1.dump());
            response.add_header("Content-Type", "application/json");
            return response;
        });
        CROW_ROUTE(CrowServer, "/misc")([this](const crow::request &req) {
            if (req.url_params.get("uuid") != nullptr && req.url_params.get("key") != nullptr &&
                authenticated(string(req.url_params.get("uuid")),
                              string(req.url_params.get("key")),
                              req.remoteIpAddress)) {
                active = true;
                nlohmann::json js1 = nlohmann::json::object();
                js1["success"] = true;
                js1["auctions"] = unsortable;
                js1["updated"] = (long long) updated;
                auto response = crow::response(200, js1.dump());
                response.add_header("Content-Type", "application/json");
                return response;
            } else {
                return crow::response(403);
            }

        });
    }

    void start() {
        running = true;
        for (int page = 0; page < 200; ++page) {
            getThreaded[page].setOpt(options::Url(
                    "https://api.hypixel.net/skyblock/auctions?page=" + to_string(page)));
            getThreaded[page].setOpt(curlpp::options::WriteStream(&getStream[page]));
            getThreaded[page].setOpt(options::TcpNoDelay(true));
        }
        thread AvgApi([this]() { get3DayAvg(); });
        thread HyApi([this]() { HyAPI(); });
        thread AuthApi([this]() { getAuth(); });
        thread ClearCache([this]() { clearCache(); });
        thread Sleep([this]() { doSleep(); });
        CrowServer.port(port).multithreaded().run();
        std::lock_guard<std::mutex> clock(exit);
        running = false;
        exitc.notify_all();
        AuthApi.join();
        Sleep.join();
        cout << "AuthAPI exited." << endl;
        AvgApi.join();
        cout << "AvgAPI exited." << endl;
        HyApi.join();
        cout << "HyAPI exited." << endl;
        ClearCache.join();
        //Server.port(port).multithreaded().run();
    }


private:
    //Objects
    crow::SimpleApp CrowServer;
    //Data
    mutex writing, lock, exit;
    condition_variable exitc;
    atomic<int> threads = 0;
    curlpp::Easy getThreaded[200];
    unordered_map<string, vector<tuple<int, string, long long, string, string>>> GlobalPrices;
    unordered_map<string, pair<string, tuple<int, string, long long, string, string>>> Cache;
    set<string> UniqueIDs;
    atomic<bool> running, active = false, sleep;
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
    static bool isLevel100(int tier, int xp) {
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

    static std::string to_roman_numerals(const int &number) {
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

    int to_tier(const string &tier) {
        return tiers[tier];
    }

    void get3DayAvg() {
        while (running) {
            lock.lock();
            ostringstream getStreams;
            getStreams << options::Url("localhost:1926/api/auction_averages");
            AveragePrice = nlohmann::json::parse(getStreams.str());
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

    void doSleep() {
        while (running) {
            if (active) {
                active = false;
            } else {
                sleep = true;
                lock.lock();
                while (!active) this_thread::sleep_for(chrono::seconds(5));
                lock.unlock();
                sleep = false;
            }
            for (int i = 0; i < 30; ++i) {
                this_thread::sleep_for(chrono::seconds(30));
                if (!running) break;
            }
        }
    }

    pair<int, long long> getPage(int page) {
        ++threads;
        auto starttime = high_resolution_clock::now();
        unordered_map<string, vector<tuple<int, string, long long, string, string>>> prices;
        unordered_map<string, pair<string, tuple<int, string, long long, string, string>>> local_cache;
        set<string> localUniqueIDs;
        do {
            getThreaded[page].perform();
        } while (curlpp::infos::ResponseCode::get(getThreaded[page]) != 200 || getStream[page].str() == "");
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
                                    second.push_back(val->second.second);
                        } else {
                            prices.insert({val->second.first, {val->second.second}});
                            localUniqueIDs.insert(val->second.first);
                        }
                    } else {
                        cout << "New entry on page: " << page << endl;
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
                                    .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes").base.end()
                            && nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
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
                            ID = petInfo["type"].get<string>() + ";" + to_string(to_tier(
                                    petInfo["tier"].get<string>()))
                                 + (isLevel100(to_tier(petInfo["tier"]),
                                               petInfo["exp"]) ? ";100" : "");

                        } else if (nbt::get_list<nbt::TagCompound>
                                           (nbtdata.at<nbt::TagList>(
                                                   "i"))[0]
                                                                                                                                                                                                                                                                               .at<nbt::TagCompound>(
                                                                                                                                                                                                                                                                                       "tag").at<nbt::TagCompound>(
                                        "ExtraAttributes")
                                                                                                                                                                                                                                                                               .at<nbt::TagString>(
                                                                                                                                                                                                                                                                                       "id").length() >
                                   8
                                   && nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                              .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                              .at<nbt::TagString>("id").substr(0, 8) == "STARRED_") {
                            ID = nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                    .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                    .at<nbt::TagString>("id").substr(8);
                        } else {
                            ID = nbt::get_list<nbt::TagCompound>(nbtdata.at<nbt::TagList>("i"))[0]
                                    .at<nbt::TagCompound>("tag").at<nbt::TagCompound>("ExtraAttributes")
                                    .at<nbt::TagString>("id");
                        }
                        if (prices.find(ID) != prices.end()) {
                            prices.find(ID)->second.emplace_back(
                                    getJson["auctions"][i]["starting_bid"],
                                    getJson["auctions"][i]["uuid"],
                                    getJson["auctions"][i]["start"],
                                    getJson["auctions"][i]["item_name"],
                                    getJson["auctions"][i]["tier"]);
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
            for (auto &i : prices) {
                auto &C = GlobalPrices[i.first];
                C.insert(C.end(), i.second.begin(), i.second.end());
            }
            Cache.merge(local_cache);
            writing.unlock();
            getStream[page].str("");
        }
        --threads;
        return {getJson["totalPages"], getJson["lastUpdated"]};
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
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                lock.unlock();
                continue;
            } else {
                sniper.clear();
                bin_full.clear();
                bin_free.clear();
                unsortable.clear();

                long long lastUpdate = information.second - 60000;
                for (int i = 1; i < information.first; ++i) {
//                      getPage(i);
                    children.emplace_back([this](int i) { getPage(i); }, i);
                }
                for (auto &child : children)
                    child.join();//This sleeps until all fetching threads are done.
                //Processing
                int count = 0;
                auto downloadtime = high_resolution_clock::now();
                for (auto &member : UniqueIDs) {
                    auto &a = GlobalPrices.find(member)->second;
                    sort(a.begin(), a.end());
                    for (auto &r : a) {
                        ++count;
                    }
                    if (GlobalPrices.find(member)->second.size() >= 2) //2 or more
                    {

                        if (AveragePrice[member]["price"].is_null()) {
                            //cout<<"COULD NOT FIND AVERAGE PRICE FOR: "<<member<<"\n";
                            if (get<0>(*(a.begin())) <=
                                int(min(get<0>(*(a.begin() + 1)) / 10 * 9,
                                        (get<0>(*(a.begin() + 1)) - margin)))) {
                                if (get<2>(*a.begin()) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(*a.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*a.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*a.begin())),
                                            make_pair("sell_price",
                                                      get<0>(*(a.begin() + 1)) - 1),
                                            make_pair("tier", (get<4>(*a.begin())))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(*a.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*a.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*a.begin())),
                                            make_pair("sell_price",
                                                      get<0>(*(a.begin() + 1)) - 1),
                                            make_pair("tier", (get<4>(*a.begin())))};
                                }
                            } else if (get<0>(*a.begin()) <=
                                       int((get<0>(*(a.begin() + 1))) / 100 * 99)) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(*a.begin()))),
                                        make_pair("item_name", (get<3>(*a.begin()))),
                                        make_pair("buy_price", get<0>(*a.begin())),
                                        make_pair("sell_price",
                                                  get<0>(*(a.begin() + 1)) - 1),
                                        make_pair("tier", (get<4>(*a.begin())))};
                            }
                        } else {
                            int minprice = min(get<0>(*(a.begin() + 1)),
                                               AveragePrice[member]["price"].get<int>());
                            if (get<0>(*a.begin()) <=
                                int(min(minprice - margin, minprice / 10 * 9))) {
                                if (get<2>(*a.begin()) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(*a.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*a.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*a.begin())),
                                            make_pair("sell_price",
                                                      min(AveragePrice[member]["price"].get<int>(),
                                                          get<0>(*(a.begin() + 1))) -
                                                      1),
                                            make_pair("tier", (get<4>(*a.begin())))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(*a.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*a.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*a.begin())),
                                            make_pair("sell_price",
                                                      min(AveragePrice[member]["price"].get<int>(),
                                                          get<0>(*(a.begin() + 1))) -
                                                      1),
                                            make_pair("tier", (get<4>(*a.begin())))};
                                }
                            } else if (get<0>(*a.begin()) <
                                       int((min(get<0>(*(a.begin() + 1)),
                                                AveragePrice[member]["price"].get<int>())) / 100 * 99)) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(*a.begin()))),
                                        make_pair("item_name", (get<3>(*a.begin()))),
                                        make_pair("buy_price", get<0>(*a.begin())),
                                        make_pair("sell_price",
                                                  min(AveragePrice[member]["price"].get<int>(),
                                                      get<0>(*(a.begin() + 1))) - 1),
                                        make_pair("tier", (get<4>(*a.begin())))};
                            }
                        }
                    } else {
                        if (AveragePrice[member]["price"].is_null()) {
                            // cout<<"UNABLE TO CLASSIFY: "<<member<<"\n";
                            unsortable[unsortable.size()] = {
                                    make_pair("uuid", (get<1>(*a.begin()))),
                                    make_pair("item_name", (get<3>(*a.begin()))),
                                    make_pair("buy_price", get<0>(*a.begin())),
                                    make_pair("tier", (get<4>(*a.begin())))};

                        } else {
                            if (get<0>(*a.begin()) <=
                                int(min(AveragePrice[member]["price"].get<int>() - margin,
                                        int(AveragePrice[member]["price"].get<int>() / 10 * 9)))) {
                                if (get<2>(*a.begin()) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(*a.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*a.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*a.begin())),
                                            make_pair("sell_price",
                                                      get<0>(*(a.begin() + 1)) - 1),
                                            make_pair("tier", (get<4>(*a.begin())))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(*a.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*a.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*a.begin())),
                                            make_pair("sell_price",
                                                      get<0>(*(a.begin() + 1)) - 1),
                                            make_pair("tier", (get<4>(*a.begin())))};
                                }
                            } else if (get<0>(*a.begin()) <
                                       int(AveragePrice[member]["price"].get<int>() / 100 * 99)) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(*a.begin()))),
                                        make_pair("item_name",
                                                  (get<3>(*a.begin()))),
                                        make_pair("buy_price", get<0>(*a.begin())),
                                        make_pair("sell_price",
                                                  get<0>(*(a.begin() + 1)) - 1),
                                        make_pair("tier", (get<4>(*a.begin())))};
                            }
                        }
                    }
                }
                auto endtime = high_resolution_clock::now();
                duration<double, std::milli> totalspeed = endtime - starttime;
                duration<double, std::milli> processingspeed = endtime - downloadtime;
                cout << "Processing speed: " << processingspeed.count()
                     << "ms, Total speed: " << totalspeed.count() << "ms" << endl;
                updated = information.second;
            }
            lock.unlock();
            this_thread::sleep_until(
                    std::chrono::system_clock::time_point(std::chrono::milliseconds{updated + 64000}));
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
