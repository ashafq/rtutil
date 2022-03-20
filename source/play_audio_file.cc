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
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <thread>

#include "RtAudio.h"
#include "circular_buffer.hh"
#include "sndfile.hh"

class PlaybackProcess {
 public:
  static constexpr std::size_t BUFFER_FACTOR = 4U;
  static constexpr std::size_t QUEUE_FACTOR = 128U * BUFFER_FACTOR;

  PlaybackProcess(SndfileHandle &&file, std::size_t frame_size)
      : file_(std::move(file)),
        circ_buffer_(QUEUE_FACTOR * file.channels() * frame_size),
        file_io_buffer_(BUFFER_FACTOR * file.channels() * frame_size, 0.0F) {}

  void start() {
    auto *buffer = file_io_buffer_.data();
    auto buffer_len = static_cast<sf_count_t>(file_io_buffer_.size());
    auto channels = static_cast<sf_count_t>(file_.channels());
    auto frames = buffer_len / channels;
    auto total_frames = file_.frames();
    std::size_t animation_counter = 0U;
    std::size_t read_counter = 0U;

    std::unique_lock lock{file_io_lock_};
    for (;;) {
      auto write_available =
          static_cast<sf_count_t>(circ_buffer_.get_write_available());

      if (write_available > buffer_len) {
        auto read_frames = file_.readf(buffer, frames);
        auto read_len = read_frames * channels;
        circ_buffer_.enqueue(buffer, read_len);
        read_counter += read_frames;
        ++animation_counter;

        // Display timeline info
        std::cout << "[ " << ("\\|/-"[animation_counter % 4]) << " Playing "
                  << (read_counter * 100) / total_frames << "% ]\r"
                  << std::flush;

        if (read_frames < frames) {
          break;
        }
      }
      // Wait until there is notification from audio process
      request_data_.wait(lock);
    }
  }

  void read_frames(float *output, std::size_t frames) {
    auto channels = static_cast<size_t>(file_.channels());
    auto read_available = circ_buffer_.get_read_available();
    auto data_needed = frames * channels;

    // Output only if there are enough data
    if (read_available > data_needed) {
      circ_buffer_.dequeue(output, data_needed);
    } else {
      // Fill the output buffer with zeros to prevent "raspberry" sound
      // when queue is empty
      auto out = std::span(output, data_needed);
      std::fill(std::begin(out), std::end(out), 0.0F);
    }

    // Send a notification to the file IO thread
    if ((io_counter_ >= (BUFFER_FACTOR * 3 / 4)) && (file_io_lock_.try_lock())) {
      io_counter_ = 0;
      request_data_.notify_one();
      file_io_lock_.unlock();
    } else {
      ++io_counter_;
    }
  }

  static int audio_callback(void *output_buffer, void * /* input_buffer */,
                            unsigned int n_frame, double /* stream_time */,
                            RtAudioStreamStatus /* status */, void *user_data) {
    auto *output = static_cast<float *>(output_buffer);
    auto *proc = static_cast<PlaybackProcess *>(user_data);
    proc->read_frames(output, n_frame);
    return 0;
  }

 private:
  SndfileHandle file_{};
  CircularBuffer<float> circ_buffer_{};
  std::vector<float> file_io_buffer_{};
  std::mutex file_io_lock_{};
  std::condition_variable request_data_{};
  std::size_t io_counter_{};
};

void play_audio_file(int api_id, int device_id, int start_channel,
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

  // Open the input file
  auto file = SndfileHandle(filename, SFM_READ);

  if (file.frames() == 0) {
    std::cerr << "Error opening file \"" << filename << "\" for reading"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Create stream parameters
  RtAudio::StreamParameters out_parameters{
      .deviceId = rt_device,
      .nChannels = static_cast<unsigned int>(file.channels()),
      .firstChannel = static_cast<unsigned int>(start_channel),
  };

  auto sample_rate = file.samplerate();
  constexpr std::size_t STREAM_FRAMESIZE = 512;
  constexpr auto STREAM_FORMAT = RTAUDIO_FLOAT32;
  unsigned int frame_size = STREAM_FRAMESIZE;
  const auto num_channels = file.channels();

  PlaybackProcess playback(std::move(file), frame_size);

  try {
    rt_audio.openStream(&out_parameters, nullptr, STREAM_FORMAT, sample_rate,
                        &frame_size, &playback.audio_callback,
                        static_cast<void *>(&playback));
  } catch (...) {
    std::cerr << "Error opening rtaudio stream" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::cout << "Play audio file: " << filename << std::endl
            << "Start channel: " << start_channel << std::endl
            << "API: " << rt_api << std::endl
            << "sample_rate: " << sample_rate << std::endl
            << "frame_size: " << frame_size << std::endl
            << "num_channels: " << num_channels << std::endl;

  std::cout << "Starting stream...\n";
  rt_audio.startStream();

  std::cout << "Starting io task...\n";
  playback.start();

  std::cout << "\nClosing stream...\n";
  rt_audio.closeStream();
}
