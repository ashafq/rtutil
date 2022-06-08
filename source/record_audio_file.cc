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

/**
 * Play an audio file to selected device
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <span>
#include <string>
#include <thread>

#include "RtAudio.h"
#include "circular_buffer.hh"
#include "sndfile.hh"

/**
 * @brief Record audio to file
 */
class RecordProcess {
 public:
  static constexpr std::size_t BUFFER_FACTOR = 4U;
  static constexpr std::size_t QUEUE_FACTOR = 4 * BUFFER_FACTOR;

  RecordProcess(SndfileHandle &&file, std::size_t frame_size)
      : file_{std::move(file)},
        circ_buffer_(QUEUE_FACTOR * file.channels() * frame_size),
        file_io_buffer_(BUFFER_FACTOR * file.channels() * frame_size, 0.0F),
        io_counter_{0} {}

  /**
   * @brief Start file io thread
   */
  void start() {
    auto *buffer = file_io_buffer_.data();
    auto buffer_len = static_cast<sf_count_t>(file_io_buffer_.size());
    auto channels = static_cast<sf_count_t>(file_.channels());
    auto frames = buffer_len / channels;
    int write_counter = 0;
    auto sample_rate = file_.samplerate();
    std::unique_lock lock{file_io_lock_};

    for (;;) {
      auto read_available = circ_buffer_.get_read_available();

      if (read_available > static_cast<std::size_t>(buffer_len)) {
        circ_buffer_.dequeue(buffer, buffer_len);
        auto write_frames = file_.writef(buffer, frames);
        write_counter += static_cast<int>(write_frames);

        // Display recording info info
        std::cout << "[ Recording " << (write_counter / sample_rate)
                  << " second(s) ]\r" << std::flush;

        file_.writeSync();
        if (write_frames < frames) {
          std::cerr << "Failed to write data..." << std::endl;
          break;
        }
      }
    }

    // Wait until there is notification from audio process
    data_ready_.wait(lock);
  }

  void write_frames(float const *input, std::size_t frames) {
    auto channels = static_cast<size_t>(file_.channels());
    circ_buffer_.enqueue(input, frames * channels);

    // Send a notification to the IO thread
    if ((io_counter_ >= BUFFER_FACTOR) && (file_io_lock_.try_lock())) {
      io_counter_ = 0;
      file_io_lock_.unlock();
      data_ready_.notify_one();
    } else {
      ++io_counter_;
    }
  }

  static int audio_callback(void * /*output_buffer*/, void *input_buffer,
                            unsigned int n_frame, double /* stream_time */,
                            RtAudioStreamStatus /* status */, void *user_data) {
    auto *input = static_cast<float *>(input_buffer);
    auto *proc = static_cast<RecordProcess *>(user_data);
    proc->write_frames(input, n_frame);
    return 0;
  }

 private:
  SndfileHandle file_{};
  CircularBuffer<float> circ_buffer_{};
  std::vector<float> file_io_buffer_{};
  std::mutex file_io_lock_{};
  std::condition_variable data_ready_{};
  std::size_t io_counter_{};
};

static int get_format_from_file_ext(std::string filename) {

  // This is a subset of formats supported by libsndfile
  static const std::unordered_map<std::string, int> fmt{
      {".wav", SF_FORMAT_WAV},   {".aiff", SF_FORMAT_AIFF},
      {".au", SF_FORMAT_AU},     {".raw", SF_FORMAT_RAW},
      {".flac", SF_FORMAT_FLAC}, {".ogg", SF_FORMAT_OGG},
  };

  namespace fs = std::filesystem;
  auto ext = fs::path(filename).extension().string();

  if (fmt.count(ext)) {
    return fmt.at(ext);
  } else {
    // Assume RAW file format
    return SF_FORMAT_RAW;
  }
}

void record_audio_file(int api_id, int device_id, int start_channel,
                       int num_channels, int sample_rate,
                       const std::string &filename) {
  // Use default API if api_id is less than zero
  auto rt_api = ([=]() {
    if (api_id < 0) {
      return RtAudio::Api::UNSPECIFIED;
    } else {
      return static_cast<RtAudio::Api>(api_id);
    }
  })();

  RtAudio rt_audio{rt_api};

  // Use default device if device_id is less than zero
  auto rt_device = ([&]() {
    if (device_id < 0) {
      return rt_audio.getDefaultOutputDevice();
    } else {
      return static_cast<unsigned int>(device_id);
    }
  })();

  // Create stream parameters
  constexpr std::size_t STREAM_FRAMESIZE = 512;
  constexpr auto STREAM_FORMAT = RTAUDIO_FLOAT32;
  unsigned int frame_size = STREAM_FRAMESIZE;
  RtAudio::StreamParameters in_parameters{
      .deviceId = rt_device,
      .nChannels = static_cast<unsigned int>(num_channels),
      .firstChannel = static_cast<unsigned int>(start_channel),
  };

  // Create a 16-bit PCM file
  auto file_format = get_format_from_file_ext(filename);
  auto file = SndfileHandle{filename, SFM_WRITE, file_format | SF_FORMAT_PCM_16,
                            num_channels, sample_rate};

  RecordProcess record{std::move(file), frame_size};

  try {
    rt_audio.openStream(nullptr, &in_parameters, STREAM_FORMAT, sample_rate,
                        &frame_size, &record.audio_callback,
                        static_cast<void *>(&record));
  } catch (...) {
    std::cerr << "Error opening rtaudio stream" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::cout << "Record audio file: " << filename << std::endl
            << "Start channel: " << start_channel << std::endl
            << "API: " << rt_api << std::endl
            << "sample_rate: " << sample_rate << std::endl
            << "frame_size: " << frame_size << std::endl
            << "num_channels: " << num_channels << std::endl;

  std::cout << "Starting stream...\n";
  rt_audio.startStream();

  std::cout << "Starting io task...\n";
  record.start();

  std::cout << "\nClosing stream...\n";
  rt_audio.closeStream();
}
