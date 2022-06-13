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

#include <array>
#include <iostream>
#include <string>
#include <vector>

// External libs
#include <RtAudio.h>

#include <tabulate/table.hpp>

void list_audio_api() {
  std::vector<RtAudio::Api> api;
  RtAudio::getCompiledApi(api);
  tabulate::Table table{};

  table.add_row({"ID", "Audio API Supported"});
  for (std::size_t i = 0; i < api.size(); ++i) {
    table.add_row({std::to_string(static_cast<int>(api[i])),
                   RtAudio::getApiDisplayName(api[i])});
  }

  table.format()
      .border_top("┅")
      .border_bottom("┅")
      .border_left("┋")
      .border_right("┋")
      .corner("◦");

  std::cout << table << std::endl;
}

template <typename DataType>
std::string vec_to_str(std::vector<DataType> const &data) {
  std::string out{};

  for (size_t i = 0; i < data.size(); ++i) {
    out.append(std::to_string(data[i]));

    if (i < (data.size() - 1)) {
      out.append(", ");
    }
  }

  return out;
}

std::string rt_sample_formats_str(std::uint32_t mask) {
  std::string out{};
  static std::array dtype{RTAUDIO_SINT8,  RTAUDIO_SINT16,  RTAUDIO_SINT24,
                          RTAUDIO_SINT32, RTAUDIO_FLOAT32, RTAUDIO_FLOAT64};
  static std::array dtype_str{"int8",  "int16",   "int24",
                              "int32", "float32", "float64"};
  size_t n = dtype.size();
  size_t m = n - 1;

  for (size_t i = 0; i < n; ++i) {
    if (dtype[i] & mask) {
      out.append(dtype_str[i]);
      if (!out.empty() && (i < m)) {
        out.append(", ");
      }
    }
  }

  return out;
}

void list_audio_device(RtAudio::Api api) {
  using namespace std::string_literals;

  RtAudio audio{api};
  tabulate::Table dev_list{};

  std::cout << "Listing audio devices using "
            << RtAudio::getApiDisplayName(audio.getCurrentApi()) << " API\n"
            << "========================================================\n";


  dev_list.add_row({"ID", "Name", "Input channel(s)", "Output channel(s)",
                    "Sample Rate(s)", "Data type(s)"});

  dev_list.format()
    .border_top("┅")
    .border_bottom("┅")
    .border_left("┋")
    .border_right("┋")
    .corner("◦");

  for (unsigned int dev = 0; dev < audio.getDeviceCount(); ++dev) {
    auto info = audio.getDeviceInfo(dev);

    if (info.probed) {
      auto name = info.name + (info.isDefaultInput ? " [Default input] " : "") +
                  (info.isDefaultOutput ? " [Default output] " : "");

      auto sample_rates = vec_to_str(info.sampleRates);

      auto data_types = rt_sample_formats_str(info.nativeFormats);

      dev_list.add_row(
          {std::to_string(dev), name, std::to_string(info.inputChannels),
           std::to_string(info.outputChannels), sample_rates, data_types});
    }

    dev_list[dev][1].format().width(30);
    dev_list[dev][2].format().width(10);
    dev_list[dev][3].format().width(10);
    dev_list[dev][4].format().width(10);
    dev_list[dev][5].format().width(10);
  }

  std::cout << dev_list << std::endl;
}
