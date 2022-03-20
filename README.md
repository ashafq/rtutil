# `rtutil`: Simple CLI PCM audio file recorder/player

A small weekend project under 10K LOC using C++20 utilizing RtAudio
and Libsndfile.

# Usage: Recording

Record a mono audio using default sound device at 16000 Hz sample rate:

```
rtutil -r rec.wav
```

Record a stereo audio file using non-default device at 48000 Hz sample
rate:

```
rtutil -d 4 -R 48000 -r rec.wav
```

# Usage: Playing

Play an audio file using default device:

```
rtutil -p music.au
```

Play an audio file using non-default device:

```
rtutil -d 4 -p music.au
```

# TODO
1. Fix queuing issues in playback. There are some dropped samples here
   and there.
2. Resample audio from file if sample rate is not supported by device.

# License
Copyright (C) 2022 Ayan Shafqat <ayan.x.shafqat@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
