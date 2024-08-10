/**
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <string>

#include <format>

#include <filesystem>

#include <vector>

#include <chrono>

#include <gtkmm.h>
#include <gdkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "logger.hxx"

extern "C"
{
#include <libavformat/avformat.h>
}

#include "vfs/vfs-file.hxx"

#include "vfs/media/metadata.hxx"

const std::vector<vfs::file::metadata_data>
vfs::detail::audio_video_metadata(const std::filesystem::path& path) noexcept
{
    std::vector<vfs::file::metadata_data> data;

    AVFormatContext* format_context = nullptr;

    if (avformat_open_input(&format_context, path.c_str(), nullptr, nullptr) != 0)
    {
        logger::error<logger::domain::vfs>("FFMPEG Could not open input file: {}", path.string());
        return data;
    }

    if (avformat_find_stream_info(format_context, nullptr) < 0)
    {
        logger::error<logger::domain::vfs>("FFMPEG Could not find stream information: {}",
                                           path.string());
        avformat_close_input(&format_context);
        return data;
    }

    // general file information
    const auto duration = std::chrono::seconds(format_context->duration / AV_TIME_BASE);
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration - hours);
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(duration - hours - minutes);
    std::string duration_dsp;
    if (hours >= std::chrono::hours(1))
    {
        duration_dsp = std::format("{}:{}:{}", hours.count(), minutes.count(), seconds.count());
    }
    else if (minutes >= std::chrono::minutes(1))
    {
        duration_dsp = std::format("{}:{}", minutes.count(), seconds.count());
    }
    else
    {
        duration_dsp = std::format("0:{}", seconds.count());
    }
    data.push_back({"Duration", duration_dsp});

    // TODO - label multiple video/audio streams
    for (u32 i = 0; i < format_context->nb_streams; ++i)
    {
        const AVCodecParameters* codec_parameters = format_context->streams[i]->codecpar;

        const AVCodec* codec = avcodec_find_decoder(codec_parameters->codec_id);
        if (codec == nullptr)
        {
            continue;
        }

        if (codec_parameters->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            data.push_back({"Video Codec", std::format("{}", codec->long_name)});

            data.push_back(
                {"Video Dimensions",
                 std::format("{} x {}", codec_parameters->width, codec_parameters->height)});

            const auto framerate =
                (f64)codec_parameters->framerate.num / (f64)codec_parameters->framerate.den;
            data.push_back({"Video Frame Rate", std::format("{:.02f}", framerate)});

            // data.push_back({"Video Bit Rate", std::format("{} kbps", format_context->bit_rate / 1000)});
            data.push_back(
                {"Video Bit Rate", std::format("{} kbps", codec_parameters->bit_rate / 1000)});
        }
        else if (codec_parameters->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            data.push_back({"Audio Codec", std::format("{}", codec->long_name)});

            data.push_back(
                {"Audio Channels", std::format("{}", codec_parameters->ch_layout.nb_channels)});

            data.push_back(
                {"Audio Sample Rate", std::format("{} Hz", codec_parameters->sample_rate)});
            data.push_back(
                {"Audio Bit Rate", std::format("{} kbps", codec_parameters->bit_rate / 1000)});
        }
    }

    avformat_close_input(&format_context);

    return data;
}
