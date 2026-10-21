# scrcpy-camera - OBS Studio Plugin

> [!WARNING]
> **Hey there!** Just a quick heads-up: this plugin was purely "vibe-coded" in some day with the help of various AI models. Because of this, the code quality isn't guaranteed and you might run into some bugs. PRs to fix issues or improve the code are more than welcome! Thanks for checking it out.

## Introduction

`scrcpy-camera` is an OBS Studio plugin that allows you to directly capture the screen or camera of an Android device using the [scrcpy](https://github.com/Genymobile/scrcpy) protocol. 

By integrating `scrcpy` directly into OBS Studio, this plugin eliminates the need to run the separate scrcpy desktop application and window-capture it. It provides a native OBS source for Android devices with low latency, hardware decoding support, and direct integration into the OBS pipeline.

> [!NOTE]
> **For Linux Users:** Native Linux support is included. `scrcpy` also offers a Video4Linux (V4L2) path that lets you use your Android device as a standard webcam. Note, however, that the V4L2 approach does **not** support capturing device audio directly; if you need audio forwarding, this plugin is the better choice. See the [scrcpy V4L2 documentation](https://github.com/Genymobile/scrcpy/blob/master/doc/v4l2.md) for more details on the alternative.

## Features

- **Direct Android Capture:** Connect to your Android device via ADB and capture its screen or camera directly in OBS.
- **Screen & Camera Modes:** Choose to mirror the device's screen or use its front/back cameras as a webcam.
- **Live Camera Controls:** Change exposure (auto with compensation and lock, or manual ISO and shutter speed), white balance, focus, zoom and torch from the source properties while streaming, without restarting the stream. See [Camera controls](#camera-controls).
- **Camera List:** Pick the camera and its size from the cameras the device actually reports, including the high-speed sizes at 120/240 fps.
- **Frame-rate Selection:** Cap the screen stream at 30/60 fps, or set the camera to 30/60 fps — camera sources additionally support 120/240 fps high-speed capture.
- **Audio Forwarding:** Optionally capture device audio (Android 11+) with support for Output, Playback (Android 13+ duplication), and Mic sources plus Opus, AAC, FLAC, and raw PCM codecs.
- **Hardware Decoding:** Utilizes FFmpeg hardware decoding (CUDA, QSV, D3D11VA, DXVA2; plus VAAPI and VDPAU on Linux) for optimized performance. The backend is chosen by querying what the selected video codec actually supports, and the one in use is named in the OBS log (`video decoding: vaapi (hevc)`), so a fallback to software is visible rather than silent.
- **Configurable Video Settings:** Adjust video codec (H.264, H.265), bitrate, maximum resolution, and camera size right from the OBS source properties.
- **Automatic Device Discovery:** Easily select from a list of connected ADB devices.

## Showcase

![Camera and Screen with scrcpy-camera](web/1.png)
*Camera and Screen with scrcpy-camera*

![Low CPU usage with active hardware decoding while using the Camera with scrcpy-camera](web/2.png)
*Low CPU usage with active hardware decoding while using the Camera with scrcpy-camera (in 720p30)*

## Requirements

- **OBS Studio:** Compatible with the latest versions of OBS Studio.
- **ADB (Android Debug Bridge):** Required to communicate with the Android device. You can specify a custom path to the ADB executable in the source properties.
- **scrcpy server:** The server component of `scrcpy` that runs on the Android device. A server with camera controls (`5.0.1-camctl.1`, built from [rahadi23/scrcpy](https://github.com/rahadi23/scrcpy), branch `feature/camera-controls`) is bundled with the plugin as `data/scrcpy-server-camctl.jar` and used by default. On Linux, building from source additionally requires the development packages listed under [Building from Source](#building-from-source).

> [!IMPORTANT]
> The **scrcpy protocol version** dropdown in the source properties selects the server. The default, **5.0.1-camctl.1 (bundled, camera controls)**, uses the bundled server, and the jar path setting is ignored. To use an upstream server instead, download the exact `scrcpy-server-vX` file from the [scrcpy release page](https://github.com/Genymobile/scrcpy/releases), rename it to `scrcpy-server.jar`, set its path, and select the matching version (5.0.1, 5.0, 4.1 or 4.0): the server compares the two with exact string equality and refuses the connection on any mismatch. The camera controls are only available with the bundled server.

## Usage

1. Connect your Android device to your computer via USB (or wireless ADB) and ensure **USB debugging** is enabled in the device's Developer Options.
2. In OBS Studio, add a new source and select **scrcpy Camera**.
3. In the source properties, configure the following:
   - **ADB executable:** Path to your `adb` executable (default is `adb.exe` on Windows, `adb` on Linux/macOS).
   - **ADB device:** Select your connected device from the dropdown, or click "Refresh device list" to scan.
   - **scrcpy-server.jar path:** Path to an upstream `scrcpy-server.jar` (ignored with the bundled server).
   - **scrcpy protocol version:** Leave at **5.0.1-camctl.1** (bundled server) unless you are deliberately using an upstream server.
   - **Video source:** Choose between **Screen** (display mirroring) and **Camera**.
   - **Frame rate:** Leave at **Default** (device default), or pick 30/60 fps. On **Camera** sources, 120 and 240 fps **High-speed** entries are also available.
   - **Video codec:** Choose between H.264 (AVC) and H.265 (HEVC).
   - **Video bitrate:** Set the desired bitrate for the stream.
   - **Max resolution:** Limit the maximum resolution of the stream.
4. If using **Camera** mode, click **Refresh camera list**, then pick the **Camera ID** and the **Camera Size**. The list comes from the device (the server is run once with `list_camera_sizes=true`) and is saved with the source. At 120/240 fps, only the high-speed sizes supporting that frame rate are listed. The live camera controls are described in [Camera controls](#camera-controls).
> [!NOTE]
   > Camera frame rates depend on what the camera exposes to the standard Android camera API — just because the stock camera app can record 60 fps (or even 4K60) does **not** mean scrcpy can capture 60 fps from it. High-speed (120/240 fps) additionally only works at size/fps combinations the device advertises. The camera list only offers the sizes the device advertises (only the high-speed ones at 120/240 fps). Some cameras are not exposed to the Android camera API at all (for example, the telephoto camera of the Galaxy Note10+). Consider raising the **Video bitrate** for high frame rates.
5. To enable **Audio Forwarding**, check "Enable Audio (Android 11+)", choose an **Audio Source** (Output, Playback for Android 13+ duplication audio, or Mic), then select your preferred audio codec (Opus, AAC, FLAC, or Raw PCM). You can also adjust the audio bitrate (default: 128 kbps).

> [!NOTE]
> Audio forwarding requires **Android 11 or higher**. The **Playback** audio source requires **Android 13 or higher**. If your device does not support the Opus encoder, try switching to AAC as a fallback.

## Camera controls

With the bundled server and a **Camera** video source, the source properties show a **Camera controls** group. Changes are applied live, while streaming: only the session settings (device, server, video source, camera, size, frame rate, codec, bitrate, audio, ...) restart the stream.

| Control | Notes |
|---|---|
| **Exposure** | **Auto**, with **Exposure compensation** (EV) and **Lock exposure**, or **Manual**, with **ISO** and **Shutter speed**. The first switch to manual starts from the current automatic values. Shutter speeds longer than one frame are not offered, since they would lower the frame rate. |
| **White balance** | Presets (Auto, Daylight, Cloudy, Incandescent, Fluorescent, ...) and **Lock white balance**. |
| **Focus** | **Continuous**, **Single** (press **Focus once**), or **Manual** with a **Focus distance** in diopters (0 = infinity, 10 = 10 cm). |
| **Zoom**, **Torch** | |

The controls adapt to what the camera reports once it is connected: ranges (ISO, shutter speed, EV, focus distance, zoom), available modes, and lock and torch availability. For example, manual exposure is not offered on cameras without manual sensor support, and manual focus is not offered on fixed-focus cameras. **Refresh camera status** shows the values currently used by the camera (the device may clamp some values).

The locks are engaged only once the automatic algorithm has converged, so that they do not freeze unconverged values. High-speed (120/240 fps) capture sessions only accept automatic modes: manual exposure, focus modes and white balance presets are not available there, and the zoom is limited slightly below the advertised maximum (on a Galaxy Note10+, the maximum zoom stalls the high-speed session).

The current controls are saved with the source and re-applied on every reconnection.

> [!NOTE]
> On some devices (seen on a Galaxy Note10+), the server crashes when the camera is opened if its command line is longer than about 256 bytes (`stack corruption detected (-fstack-protector)`). The bundled server therefore receives its options in the `SCRCPY_SERVER_OPTIONS` environment variable rather than on the command line. Upstream servers still receive them on the command line: with many options (e.g. camera and audio), they may hit this limit on such devices.

## Building from Source

This project uses CMake and builds on Windows, macOS, and Linux. To build the plugin, ensure you have CMake, a C compiler (Visual Studio, GCC, or Clang), and the OBS Studio dependencies configured, then generate and build:

```bash
cmake -B build
cmake --build build
```

### Linux (Ubuntu/Debian)

First install the required build dependencies:

```bash
sudo apt install build-essential cmake ninja-build \
  libobs-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev libsimde-dev
```

The Linux build uses the `ubuntu-x86_64` CMake preset (Ninja generator):

```bash
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
```

The resulting plugin is built as `build_x86_64/scrcpy-camera.so`; outputs are also staged under `build_x86_64/rundir/` so you can point a local OBS install at it. On Linux, hardware decoding uses the VAAPI and VDPAU backends (plus CUDA where available).

### Bundled server

The bundled server is built from [rahadi23/scrcpy](https://github.com/rahadi23/scrcpy) (branch `feature/camera-controls`, with JDK 17 and the Android SDK platform 36):

```bash
./gradlew :server:assembleRelease
cp server/build/outputs/apk/release/server-release-unsigned.apk <scrcpy-camera>/data/scrcpy-server-camctl.jar
```

Re-run the CMake configure step after updating the data files.

### Tests

The camera controls protocol and the camera list parser have no OBS dependency, and are tested on their own:

```bash
gcc -std=c11 -Wall -Wextra -Isrc tests/test-camera-protocol.c src/camera-protocol.c -o test-camera-protocol && ./test-camera-protocol
gcc -std=c11 -Wall -Wextra -Isrc tests/test-camera-list.c src/camera-list.c -o test-camera-list && ./test-camera-list
```

## License

This project is licensed under the GNU General Public License v2 (or later) - see the [LICENSE](LICENSE) file for details.

Copyright (C) 2026 NanKill <nankill@nankill.xyz>
