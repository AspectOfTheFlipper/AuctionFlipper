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
#include <simdjson.h>
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
    mutex cachewrite, newelement, lock, exit;
    condition_variable exitc;
    atomic<int> threads = 0;
    curlpp::Easy getThreaded[200];
    unordered_map<string, pair<vector<tuple<int, string, long long, string, string>>, unique_ptr<mutex>>> GlobalPrices;
    unordered_map<string, pair<string, tuple<int, string, long long, string, string>>> Cache;
    vector<string> UniqueIDs;
    atomic<bool> running, active = false, sleep;
//    nlohmann::json AveragePrice;
    simdjson::ondemand::parser getParser[200];
    unordered_map<string, int> AveragePrice;
    nlohmann::json sniper = nlohmann::json::array(), bin_full = nlohmann::json::array(),
            bin_free = nlohmann::json::array(), unsortable = nlohmann::json::array();
    atomic<long long> updated = 0;
    unordered_map<string, string> Auth;
    ostringstream getStream[200];
    unordered_map<string, int> Enchant_Prices = {
            {"TELEKINESIS;1",           100},
            {"SHARPNESS;1",             100},
            {"SHARPNESS;2",             200},
            {"SHARPNESS;3",             300},
            {"SHARPNESS;4",             500},
            {"SHARPNESS;5",             1000},
            {"SMITE;1",                 100},
            {"SMITE;2",                 200},
            {"SMITE;3",                 300},
            {"SMITE;4",                 500},
            {"SMITE;5",                 1000},
            {"BANE_OF_ARTHROPODS;1",    100},
            {"BANE_OF_ARTHROPODS;2",    200},
            {"BANE_OF_ARTHROPODS;3",    300},
            {"BANE_OF_ARTHROPODS;4",    500},
            {"BANE_OF_ARTHROPODS;5",    1000},
            {"KNOCKBACK;1",             0},
            {"KNOCKBACK;2",             0},
            {"FIRE_ASPECT;1",           100},
            {"FIRE_ASPECT;2",           200},
            {"EXPERIENCE;1",            100},
            {"EXPERIENCE;2",            200},
            {"EXPERIENCE;3",            1000},
            {"LOOTING;1",               100},
            {"LOOTING;2",               200},
            {"LOOTING;3",               1000},
            {"SCAVENGER;1",             100},
            {"SCAVENGER;2",             200},
            {"SCAVENGER;3",             1000},
            {"LUCK;1",                  100},
            {"LUCK;2",                  200},
            {"LUCK;3",                  300},
            {"LUCK;4",                  500},
            {"LUCK;5",                  1000},
            {"CUBISM;1",                100},
            {"CUBISM;2",                200},
            {"CUBISM;3",                300},
            {"CUBISM;4",                500},
            {"CUBISM;5",                1000},
            {"CLEAVE;1",                100},
            {"CLEAVE;2",                200},
            {"CLEAVE;3",                300},
            {"CLEAVE;4",                500},
            {"CLEAVE;5",                1000},
            {"LIFE_STEAL;1",            100},
            {"LIFE_STEAL;2",            200},
            {"LIFE_STEAL;3",            100},
            {"GIANT_KILLER;1",          100},
            {"GIANT_KILLER;2",          200},
            {"GIANT_KILLER;3",          300},
            {"GIANT_KILLER;4",          500},
            {"GIANT_KILLER;5",          1000},
            {"CRITICAL;1",              100},
            {"CRITICAL;2",              200},
            {"CRITICAL;3",              300},
            {"CRITICAL;4",              500},
            {"CRITICAL;5",              1000},
            {"FIRST_STRIKE;1",          500},
            {"FIRST_STRIKE;2",          1000},
            {"FIRST_STRIKE;3",          3000},
            {"FIRST_STRIKE;4",          10000},
            {"ENDER_SLAYER;1",          100},
            {"ENDER_SLAYER;2",          200},
            {"ENDER_SLAYER;3",          300},
            {"ENDER_SLAYER;4",          500},
            {"ENDER_SLAYER;5",          1000},
            {"IMPALING;1",              1000},
            {"IMPALING;2",              2000},
            {"IMPALING;3",              5000},
            {"EXECUTE;1",               100},
            {"EXECUTE;2",               200},
            {"EXECUTE;3",               300},
            {"EXECUTE;4",               500},
            {"EXECUTE;5",               1000},
            {"THUNDERLORD;1",           100},
            {"THUNDERLORD;2",           200},
            {"THUNDERLORD;3",           300},
            {"THUNDERLORD;4",           500},
            {"THUNDERLORD;5",           1000},
            {"LETHALITY;1",             100},
            {"LETHALITY;2",             200},
            {"LETHALITY;3",             300},
            {"LETHALITY;4",             500},
            {"LETHALITY;5",             1000},
            {"SYPHON;1",                100},
            {"SYPHON;2",                200},
            {"SYPHON;3",                300},
            {"VAMPIRISM;1",             100},
            {"VAMPIRISM;2",             200},
            {"VAMPIRISM;3",             300},
            {"VAMPIRISM;4",             500},
            {"VAMPIRISM;5",             1000},
            {"VENOMOUS;1",              100},
            {"VENOMOUS;2",              200},
            {"VENOMOUS;3",              300},
            {"VENOMOUS;4",              500},
            {"VENOMOUS;5",              1000},
            {"TRIPLE_STRIKE;1",         100},
            {"TRIPLE_STRIKE;2",         200},
            {"TRIPLE_STRIKE;3",         300},
            {"TRIPLE_STRIKE;4",         500},
            {"THUNDERBOLT;1",           100},
            {"THUNDERBOLT;2",           200},
            {"THUNDERBOLT;3",           300},
            {"THUNDERBOLT;4",           500},
            {"THUNDERBOLT;5",           1000},
            {"PROSECUTE;1",             100},
            {"PROSECUTE;2",             200},
            {"PROSECUTE;3",             300},
            {"PROSECUTE;4",             500},
            {"PROSECUTE;5",             1000},
            {"TITAN_KILLER;1",          100},
            {"TITAN_KILLER;2",          200},
            {"TITAN_KILLER;3",          300},
            {"TITAN_KILLER;4",          500},
            {"TITAN_KILLER;5",          1000},
            {"PROJECTILE_PROTECTION;1", 100},
            {"PROJECTILE_PROTECTION;2", 200},
            {"PROJECTILE_PROTECTION;3", 300},
            {"PROJECTILE_PROTECTION;4", 500},
            {"PROJECTILE_PROTECTION;5", 1000},
            {"PROTECTION;1",            100},
            {"PROTECTION;2",            200},
            {"PROTECTION;3",            300},
            {"PROTECTION;4",            500},
            {"PROTECTION;5",            1000},
            {"BLAST_PROTECTION;1",      100},
            {"BLAST_PROTECTION;2",      200},
            {"BLAST_PROTECTION;3",      300},
            {"BLAST_PROTECTION;4",      500},
            {"BLAST_PROTECTION;5",      1000},
            {"FIRE_PROTECTION;1",       100},
            {"FIRE_PROTECTION;2",       200},
            {"FIRE_PROTECTION;3",       300},
            {"FIRE_PROTECTION;4",       500},
            {"FIRE_PROTECTION;5",       1000},
            {"RESPIRATION;1",           100},
            {"RESPIRATION;2",           200},
            {"RESPIRATION;3",           300},
            {"AQUA_AFFINITY;1",         100},
            {"THORNS;1",                100},
            {"THORNS;2",                200},
            {"THORNS;3",                300},
            {"GROWTH;1",                100},
            {"GROWTH;2",                200},
            {"GROWTH;3",                300},
            {"GROWTH;4",                500},
            {"GROWTH;5",                1000},
            {"EFFICIENCY;1",            100},
            {"EFFICIENCY;2",            200},
            {"EFFICIENCY;3",            300},
            {"EFFICIENCY;4",            500},
            {"EFFICIENCY;5",            1000},
            {"EFFICIENCY;6",            0},
            {"SMELTING_TOUCH;1",        100},
            {"SILK_TOUCH;1",            100},
            {"POWER;1",                 100},
            {"POWER;2",                 200},
            {"POWER;3",                 300},
            {"POWER;4",                 500},
            {"POWER;5",                 1000},
            {"PUNCH;1",                 100},
            {"PUNCH;2",                 200},
            {"FLAME;1",                 100},
            {"INFINITE_QUIVER;1",       100},
            {"INFINITE_QUIVER;2",       200},
            {"INFINITE_QUIVER;3",       300},
            {"INFINITE_QUIVER;4",       500},
            {"INFINITE_QUIVER;5",       1000},
            {"SNIPE;1",                 100},
            {"SNIPE;2",                 200},
            {"SNIPE;3",                 300},
            {"AIMING;1",                100},
            {"AIMING;2",                200},
            {"AIMING;3",                300},
            {"AIMING;4",                500},
            {"AIMING;5",                1000},
            {"CHANCE;1",                100},
            {"CHANCE;2",                200},
            {"CHANCE;3",                300},
            {"PIERCING;1",              100},
            {"TRUE_PROTECTION;1",       890000},
            {"ENDER_SLAYER;6",          1500000},
            {"DRAGON_HUNTER;1",         1000000},
            {"DRAGON_HUNTER;2",         2000000},
            {"DRAGON_HUNTER;3",         4000000},
            {"DRAGON_HUNTER;4",         8000000},
            {"DRAGON_HUNTER;5",         16000000}};
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

    void BazaarAPI() {
        while (running) {
            cURLpp::Easy fetch;
        }
    }

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
                return xp >= 18608500;
            default:
                return xp >= 25353230;
        }
    }


    int to_tier(const string &tier) {
        return tiers[tier];
    }

    void get3DayAvg() {
        while (running) {
            lock.lock();
            AveragePrice.clear();
            ostringstream getStreams;
            getStreams << options::Url("localhost:1926/api/auction_averages");
            simdjson::ondemand::parser AvgParser;
            simdjson::padded_string AvgPadded = getStreams.str();
            simdjson::ondemand::document AvgDoc = AvgParser.iterate(AvgPadded);
            auto AvgObj = AvgDoc.get_object();
            for (auto field : AvgObj) {
                AveragePrice.insert({string(field.unescaped_key().value()),
                                     int(field.value().find_field("price").get_int64())});
            }
//            AveragePrice = nlohmann::json::parse(getStreams.str());
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
//        unordered_map<string, vector<tuple<int, string, long long, string, string>>> prices;
//        unordered_map<string, pair<string, tuple<int, string, long long, string, string>>> local_cache;
        vector<string> localUniqueIDs;
        do {
            getThreaded[page].perform();
        } while (curlpp::infos::ResponseCode::get(getThreaded[page]) != 200 || getStream[page].str() == "");
        auto downloadtime = high_resolution_clock::now();
        simdjson::padded_string getPadded;
        simdjson::ondemand::document getJson;
        try {
            getPadded = getStream[page].str();
            getJson = getParser[page].iterate(getPadded);
        } catch (...) {
            cout << "Failed to parse JSON\n";
            return {0, 0};
        }
//        auto getJson = nlohmann::json::parse(getStream[page].str());
        int size = 0;
        if (!getJson["success"].is_null() && getJson["success"].get_bool()) {
            auto auctions = getJson["auctions"].get_array();
            for (auto i : auctions) {
                ++size;
//                cout<<"Analysing "<<i<<" on page "<<page<<'\n';
                try {
                    if (i["bin"].get_bool()) {
                        auto val = Cache.find(string(i["uuid"].get_string().value()));
                        if (val != Cache.end()) {
                            if (GlobalPrices.find(val->second.first) != GlobalPrices.end()) {
                                GlobalPrices.find(val->second.first)->second.second->lock();
                                GlobalPrices.find(val->second.first)->
                                        second.first.push_back(val->second.second);
                                GlobalPrices.find(val->second.first)->second.second->unlock();
                            } else {
                                newelement.lock();
                                GlobalPrices.insert({val->second.first,
                                                     pair<vector<tuple<int, string,
                                                             long long, string, string>>, unique_ptr<mutex>>
                                                             ({val->second.second}, new mutex)});
                                localUniqueIDs.push_back(val->second.first);
                                newelement.unlock();
                            }
                        } else {
                            //                        cout << "New entry on page: " << page << endl;
                            auto c = cppcodec::base64_rfc4648::decode(
                                    string(i["item_bytes"].get_string().value()));
                            string d(c.begin(), c.end());

                            nbt::NBT nbtdata;
                            istringstream str(d);
                            zstr::istream decoded(str);
                            nbtdata.decode(decoded);
                            string ID;
                            int value = 0;
                            //                        cout<<nbtdata<<endl;
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
                                                       "i"))[0].at<nbt::TagCompound>("tag").at<nbt::TagCompound>(
                                    "ExtraAttributes").at<nbt::TagString>("id").length() > 8
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
                            if (GlobalPrices.find(ID) != GlobalPrices.end()) {
                                GlobalPrices.find(ID)->second.second->lock();
                                GlobalPrices.find(ID)->second.first.emplace_back(
                                        i["starting_bid"].get_int64(),
                                        string(i["uuid"].get_string().value()),
                                        i["start"].get_int64(),
                                        string(i["item_name"].get_string().value()),
                                        string(i["tier"].get_string().value()));
                                GlobalPrices.find(ID)->second.second->unlock();
                            } else {
                                newelement.lock();
                                GlobalPrices.insert({ID, pair<vector<tuple<int, string,
                                        long long, string, string>>, unique_ptr<mutex>>(
                                        {tuple<int, string, long long, string, string>(
                                                i["starting_bid"].get_int64(),
                                                string(i["uuid"].get_string().value()),
                                                i["start"].get_int64(),
                                                string(i["item_name"].get_string().value()),
                                                string(i["tier"].get_string().value()))}, new mutex)});
                                localUniqueIDs.push_back(ID);
                                newelement.unlock();
                            }
                            cachewrite.lock();
                            Cache.insert({string(i["uuid"].get_string().value()), {
                                    ID, tuple<int, string, long long, string, string>(
                                            int(i["starting_bid"].get_int64()),
                                            string(i["uuid"].get_string().value()),
                                            (long long) (i["start"].get_int64()),
                                            string(i["item_name"].get_string().value()),
                                            string(i["tier"].get_string().value()))}});
                            cachewrite.unlock();
                        }
                    }
                } catch (...) {

                }

            }
            UniqueIDs.insert(UniqueIDs.end(), localUniqueIDs.begin(), localUniqueIDs.end());
//            for (auto &i : prices) {
//                auto &C = GlobalPrices[i.first];
//                C.insert(C.end(), i.second.begin(), i.second.end());
//            }
//            Cache.merge(local_cache);
            getStream[page].str("");
        }
        --threads;
        return {getJson["totalPages"].get_int64(), getJson["lastUpdated"].get_int64()};
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
                auto UniqueIDsSet = set<string>(UniqueIDs.begin(), UniqueIDs.end());
                for (auto &member : UniqueIDsSet) {
                    auto &a = GlobalPrices.find(member)->second.first;
                    sort(a.begin(), a.end());
                    for (auto &r : a) {
                        ++count;
                    }
                    if (a.size() >= 2) //2 or more
                    {

                        if (AveragePrice.find(member) == AveragePrice.end()) {
                            cout << "COULD NOT FIND AVERAGE PRICE FOR: " << member << "\n";
                            unsortable[unsortable.size()] = {
                                    make_pair("uuid", (get<1>(*a.begin()))),
                                    make_pair("item_name", (get<3>(*a.begin()))),
                                    make_pair("buy_price", get<0>(*a.begin())),
                                    make_pair("tier", (get<4>(*a.begin())))};
                        } else {
                            int minprice = min(get<0>(*(a.begin() + 1)),
                                               AveragePrice[member]);
                            if (get<0>(*a.begin()) <=
                                min(minprice - margin, minprice / 10 * 9)) {
                                if (get<2>(*a.begin()) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(*a.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*a.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*a.begin())),
                                            make_pair("sell_price",
                                                      minprice),
                                            make_pair("tier", (get<4>(*a.begin())))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(*a.begin()))),
                                            make_pair("item_name",
                                                      (get<3>(*a.begin()))),
                                            make_pair("buy_price",
                                                      get<0>(*a.begin())),
                                            make_pair("sell_price",
                                                      min(AveragePrice[member],
                                                          get<0>(*(a.begin() + 1))) -
                                                      1),
                                            make_pair("tier", (get<4>(*a.begin())))};
                                }
                            } else if (get<0>(*a.begin()) <
                                       int((min(get<0>(*(a.begin() + 1)),
                                                AveragePrice[member])) / 100 * 99)) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(*a.begin()))),
                                        make_pair("item_name", (get<3>(*a.begin()))),
                                        make_pair("buy_price", get<0>(*a.begin())),
                                        make_pair("sell_price",
                                                  min(AveragePrice[member],
                                                      get<0>(*(a.begin() + 1))) - 1),
                                        make_pair("tier", (get<4>(*a.begin())))};
                            }
                        }
                    } else {
                        // cout<<"UNABLE TO CLASSIFY: "<<member<<"\n";
                        unsortable[unsortable.size()] = {
                                make_pair("uuid", (get<1>(*a.begin()))),
                                make_pair("item_name", (get<3>(*a.begin()))),
                                make_pair("buy_price", get<0>(*a.begin())),
                                make_pair("tier", (get<4>(*a.begin())))};

                    }
                }
                auto endtime = high_resolution_clock::now();
                duration<double, std::milli> totalspeed = endtime - starttime;
                duration<double, std::milli> processingspeed = endtime - downloadtime;
                cout << "Processing speed: " << processingspeed.count()
                     << "ms, Total speed: " << totalspeed.count() << "ms" << endl;
                updated = information.second;
                for (auto &i : GlobalPrices) {
                    i.second.first.clear();
                }
            }
            lock.unlock();
            this_thread::sleep_until(
                    std::chrono::system_clock::time_point(std::chrono::milliseconds{updated + 69500}));
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
