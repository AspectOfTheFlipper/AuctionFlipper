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
#include <sys/wait.h>
#include <sys/types.h>
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
        CROW_ROUTE(CrowServer, "/sniper")([this] {
            return (nlohmann::json::array({{"success",  true},
                                           {"auctions", sniper}}).dump());
        });
        CROW_ROUTE(CrowServer, "/bin_full")([this] {
            return (nlohmann::json::array({{"success",  true},
                                           {"auctions", bin_full}}).dump());
        });
        CROW_ROUTE(CrowServer, "/bin_free")([this] {
            return (nlohmann::json::array({{"success",  true},
                                           {"auctions", bin_free}}).dump());
        });
        CROW_ROUTE(CrowServer, "/misc")([this] {
            return (nlohmann::json::array({{"success",  true},
                                           {"auctions", unsortable}}).dump());
        });
    }

    void start() {
        running = true;
        thread AvgApi([this]() { get3DayAvg(); });
        thread HyApi([this]() { HyAPI(); });
        CrowServer.port(port).multithreaded().run();
        HyApi.join();
        AvgApi.join();
        //Server.port(port).multithreaded().run();
    }


private:
    //Objects
    crow::SimpleApp CrowServer;
    //Data
    mutex writing, lock;
    atomic<int> threads = 0;
    multimap<string, tuple<int, string, long long, string, string>> GlobalPrices;
    set<string> UniqueIDs;
    atomic<bool> running;
    nlohmann::json AveragePrice;
    nlohmann::json sniper, bin_full,
            bin_free, unsortable;
    atomic<long long> updated = 0;
    int port = 8080;
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
            this_thread::sleep_for(chrono::hours(3));
        }
    }

    pair<int, long long> getPage(int page) {
        ++threads;
        ostringstream getStream;
        multimap<string, tuple<int, string, long long, string, string>> prices;
        set<string> localUniqueIDs;
        getStream << options::Url("https://api.hypixel.net/skyblock/auctions?page=" + to_string(page));
        auto getJson = nlohmann::json::parse(getStream.str());
        int size = getJson["auctions"].size();
        if (getJson["success"].type() == nlohmann::detail::value_t::boolean && getJson["success"].get<bool>()) {
            for (int i = 0; i < size; ++i) {
//                cout<<"Analysing "<<i<<" on page "<<page<<'\n';
                if (getJson["auctions"][i]["bin"].type() == nlohmann::detail::value_t::boolean) {
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
                    prices.insert({ID, tuple<int, string, long long, string, string>(
                            getJson["auctions"][i]["starting_bid"],
                            getJson["auctions"][i]["uuid"],
                            getJson["auctions"][i]["start"],
                            getJson["auctions"][i]["item_name"],
                            getJson["auctions"][i]["tier"])});
                    localUniqueIDs.insert(ID);
                }

            }
            writing.lock();
            UniqueIDs.merge(localUniqueIDs);
            GlobalPrices.merge(prices);
            writing.unlock();
        }
        --threads;
        return pair<int, long long>(getJson["totalPages"], getJson["lastUpdated"]);
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
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
                    children.emplace_back([this](int i) { getPage(i); }, i);
                }
                for (auto &child : children)
                    child.join();//This sleeps until all fetching threads are done.
                //Processing
                auto downloadtime = high_resolution_clock::now();
                for (auto &member : UniqueIDs) {
                    if (GlobalPrices.find(member)->first == next(GlobalPrices.find(member))->first) //2 or more
                    {
                        if (AveragePrice[member]["price"].is_null()) {
                            //cout<<"COULD NOT FIND AVERAGE PRICE FOR: "<<member<<"\n";
                            if (get<0>(GlobalPrices.find(member)->second) <=
                                get<0>(next(GlobalPrices.find(member))->second) - margin) {
                                if (get<2>(GlobalPrices.find(member)->second) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                            make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                            make_pair("buy_price",
                                                      to_string(get<0>(GlobalPrices.find(member)->second))),
                                            make_pair("sell_price",
                                                      to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                            make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                            make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                            make_pair("buy_price",
                                                      to_string(get<0>(GlobalPrices.find(member)->second))),
                                            make_pair("sell_price",
                                                      to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                            make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                                }
                            } else if (get<0>(GlobalPrices.find(member)->second) <=
                                       get<0>(next(GlobalPrices.find(member))->second)) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                        make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                        make_pair("buy_price", to_string(get<0>(GlobalPrices.find(member)->second))),
                                        make_pair("sell_price",
                                                  to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                        make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                            }
                        } else {
                            if (get<0>(GlobalPrices.find(member)->second) <=
                                min(get<0>(next(GlobalPrices.find(member))->second),
                                    AveragePrice[member]["price"].get<int>()) - margin) {
                                if (get<2>(GlobalPrices.find(member)->second) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                            make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                            make_pair("buy_price",
                                                      to_string(get<0>(GlobalPrices.find(member)->second))),
                                            make_pair("sell_price",
                                                      to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                            make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                            make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                            make_pair("buy_price",
                                                      to_string(get<0>(GlobalPrices.find(member)->second))),
                                            make_pair("sell_price",
                                                      to_string(AveragePrice[member]["price"].get<int>() - 1)),
                                            make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                                }
                            } else if (get<0>(GlobalPrices.find(member)->second) <=
                                       min(get<0>(next(GlobalPrices.find(member))->second),
                                           AveragePrice[member]["price"].get<int>())) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                        make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                        make_pair("buy_price", to_string(get<0>(GlobalPrices.find(member)->second))),
                                        make_pair("sell_price",
                                                  to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                        make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                            }
                        }
                    } else {
                        if (AveragePrice[member].is_null()) {
                            // cout<<"UNABLE TO CLASSIFY: "<<member<<"\n";
                            unsortable[unsortable.size()] = {
                                    make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                    make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                    make_pair("buy_price", to_string(get<0>(GlobalPrices.find(member)->second))),
                                    make_pair("sell_price",
                                              to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                    make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};

                        } else {
                            if (get<0>(GlobalPrices.find(member)->second) <=
                                AveragePrice[member]["price"].get<int>() - margin) {
                                if (get<2>(GlobalPrices.find(member)->second) >= lastUpdate) {
                                    sniper[sniper.size()] = {
                                            make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                            make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                            make_pair("buy_price",
                                                      to_string(get<0>(GlobalPrices.find(member)->second))),
                                            make_pair("sell_price",
                                                      to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                            make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                                } else {
                                    bin_full[bin_full.size()] = {
                                            make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                            make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                            make_pair("buy_price",
                                                      to_string(get<0>(GlobalPrices.find(member)->second))),
                                            make_pair("sell_price",
                                                      to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                            make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                                }
                            } else if (get<0>(GlobalPrices.find(member)->second) <=
                                       AveragePrice[member]["price"].get<int>()) {
                                bin_free[bin_free.size()] = {
                                        make_pair("uuid", (get<1>(GlobalPrices.find(member)->second))),
                                        make_pair("item_name", (get<3>(GlobalPrices.find(member)->second))),
                                        make_pair("buy_price", to_string(get<0>(GlobalPrices.find(member)->second))),
                                        make_pair("sell_price",
                                                  to_string(get<0>(next(GlobalPrices.find(member))->second) - 1)),
                                        make_pair("tier", (get<4>(GlobalPrices.find(member)->second)))};
                            }
                        }
                    }
                }
                auto endtime = high_resolution_clock::now();
                duration<double, std::milli> processingspeed = endtime - downloadtime;
                cout << processingspeed.count() << endl;
            }
            lock.unlock();
            this_thread::sleep_until(
                    std::chrono::system_clock::time_point(std::chrono::milliseconds{updated + 60000}));
        }
    }

};

int main() {
    //    Pistache::Address addr(Pistache::Ipv4::any(), Pistache::Port(9080));
    Server entrypoint;
    entrypoint.initialize();
    entrypoint.start();
    cURLpp::terminate();
    return 0;
}