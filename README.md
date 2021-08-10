# This is a BIN Sniper made for iCheppy

Instructions for the building and installation of this project down below

Dependencies:
CMake, libcurl(dev), curlpp(dev)m Crow(cpp), zlib, cpp_codec, nlohmann::json, and libraries contained within the repo. Packages can be found for your specific distribution of Linux, as well as their installation instructions with a google search.

Build instructions:
1. Clone this repo
2. cd AuctionFlipper (or whatever you name this directory)
3. mkdir build
4. cd build
5. cmake .. -DCMAKE_BUILD_TYPE=Release
6. make
Run the binary executable for the server. Make sure to change any settings you need within main.cpp, including port(I suggest 80 as I haven't figured out HTTPS yet)
