/**
 * This file is part of rtutils distribution
 *
 * Copyright (C) 2022 Ayan Shafqat <ayan.x.shafqat@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include <iostream>
#include <string>

// Third party
#include "RtAudio.h"
#include "cxxopts.hpp"
#include "sndfile.hh"

void list_audio_api();
void list_audio_device(RtAudio::Api);
void play_audio_file(int api_id, int device_id, int start_channel,
                     const std::string &filename);
void record_audio_file(int api_id, int device_id, int start_channel,
                       int num_channels, int sample_rate,
                       const std::string &filename);

constexpr std::string_view RTUTIL_VERSION = "1.0.0";

static std::string cxxopt_version_string() {
  return std::to_string(CXXOPTS__VERSION_MAJOR) + "." +
         std::to_string(CXXOPTS__VERSION_MINOR) + "." +
         std::to_string(CXXOPTS__VERSION_PATCH);
}

int main(int argc, char **argv) {
  cxxopts::Options options("rtutil", "Utility to record/play audio file");
  cxxopts::ParseResult result{};

  options.add_options()  // List options
      ("d,device", "Device ID to play or record",
       cxxopts::value<int>()->default_value("-1"))           //
      ("l,list-device", "List enumerated device list")       //
      ("L,list-device-api", "List compiled supported APIs")  //
      ("A,select-api", "Select audio API",
       cxxopts::value<int>()->default_value("-1"))  //
      ("s,start-channel", "Start channel offset",
       cxxopts::value<int>()->default_value("0"))  //
      ("c,channels", "Number of channels",
       cxxopts::value<int>()->default_value("1"))  //
      ("R,rate", "Sample rate [for-recording]",
       cxxopts::value<int>()->default_value("16000"))  //
      ("r,record", "Record an audio file",
       cxxopts::value<std::string>())  //
      ("p,play", "Play an audio file",
       cxxopts::value<std::string>())         //
      ("v,version", "Print program version")  //
      ("h,help", "Print usage and exit");

  try {
    result = options.parse(argc, argv);
  } catch (...) {
    std::cout << options.help() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    std::exit(EXIT_SUCCESS);
  } else if (result.count("version")) {
    std::cout << "RtUtil version: " << RTUTIL_VERSION << std::endl
              << "RtAudio version: " << RtAudio::getVersion() << std::endl
              << "LibSndFile version: " << sf_version_string() << std::endl
              << "CxxOpts version: " << cxxopt_version_string() << std::endl;
    std::exit(EXIT_SUCCESS);
  } else if (result.count("list-device-api")) {
    list_audio_api();
  } else if (result.count("list-device")) {
    auto api = RtAudio::Api(result["select-api"].as<int>());
    list_audio_device(api);
  } else if (result.count("play")) {
    auto api = RtAudio::Api(result["select-api"].as<int>());
    auto dev = result["device"].as<int>();
    auto start_channel = result["start-channel"].as<int>();
    auto filename = result["play"].as<std::string>();
    play_audio_file(api, dev, start_channel, filename);
  } else if (result.count("record")) {
    auto api = RtAudio::Api(result["select-api"].as<int>());
    auto dev = result["device"].as<int>();
    auto start_channel = result["start-channel"].as<int>();
    auto num_channels = result["channels"].as<int>();
    auto sample_rate = result["rate"].as<int>();
    auto filename = result["record"].as<std::string>();
    record_audio_file(api, dev, start_channel, num_channels, sample_rate,
                      filename);
  } else {
    std::cout << "Invalid option\n" << options.help() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
