/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIO_PROXY_H
#define AUDIO_PROXY_H

#include <system/audio.h>
#include <hardware/hardware.h>
#include <hardware/audio.h>
#include <hardware/audio_alsaops.h>

#include <audio_utils/resampler.h>

#include "alsa_device_profile.h"
#include "alsa_device_proxy.h"
#include "alsa_logging.h"

#include "audio_streams.h"
#include "audio_usages.h"
#include "audio_devices.h"
#include "audio_offload.h"

#include "audio_pcm.h"
#include "audio_mixer.h"
#include "audio_abox.h"
#include "audio_streamconfig.h"
#include "audio_board_info.h"

/* Call parameter integer value bit-field structure */
struct call_param {
    uint32_t device      : 6;   // handset, speaker, bt-sco, usb
    uint32_t rate_band   : 2;   // NB, WB, SWB
    uint32_t call_type   : 5;   // Call-type GSM, CDMA, IMS, ETC(3G)
    uint32_t tty_mode    : 2;   // VCO, HCO, FULL
    uint32_t mic_num     : 3;   // Mic count
    uint32_t channel     : 4;   // channel count mono, stereo
    uint32_t reserved    : 7;   // reserved
    uint32_t special     : 3;   // special any ODM specific controls
};

/* Data Structure for Audio Proxy */
struct audio_proxy_stream
{
    audio_stream_type stream_type;
    audio_usage       stream_usage;

    // Real configuration for PCM/Compress Device
    int sound_card;
    int sound_device;

    /* DMA pcm handle is required for triggering Virtual node use-case */
    struct pcm *dma_pcm;
    /* Can be actual DMA handle or virtual pcm depends upon scenario */
    struct pcm *pcm;
    struct pcm_config pcmconfig;

    // Offload Specific
    struct compress *compress;
    struct compr_config comprconfig;

    int nonblock_flag;
    int ready_new_metadata;
    struct compr_gapless_mdata offload_metadata;

#ifdef SUPPORT_USB_OFFLOAD
    // USB Specific
    alsa_device_profile *usb_profile;
    alsa_device_proxy   *usb_proxy;
    bool   need_channelpadding;
#endif

    // Common
    unsigned int            requested_sample_rate;
    audio_channel_mask_t    requested_channel_mask;
    audio_format_t          requested_format;

    float vol_left, vol_right;

    uint64_t frames; /* total frames written, not cleared when entering standby */


    // Channel Conversion & Resample for Recording
    bool   need_channelconversion;
    bool   need_resampling;

    int16_t* actual_read_buf;
    int      actual_read_status;
    size_t   actual_read_buf_size;
    size_t   read_buf_frames;

    void *   proc_buf_out;
    int      proc_buf_size;

    // Resampler
#ifdef SEC_AUDIO_RESAMPLER
    void * resampler;
    void * conversion_buffer;
    size_t conversion_buffer_size;
    size_t conversion_frames;
#else
    struct resampler_itfe *             resampler;
#endif
    struct resampler_buffer_provider    buf_provider;


#ifdef SUPPORT_STHAL_INTERFACE
    int soundtrigger_handle;
#ifdef SEAMLESS_DUMP
    FILE *fp;
#endif
#endif

    bool   need_update_pcm_config;
    bool   skip_ch_convert;
#ifdef SEC_AUDIO_SAMSUNGRECORD
    bool   skip_format_convert;
#endif
#ifdef SEC_AUDIO_DUMP
    int input_count;
#endif
};

struct audio_proxy
{
    // Audio Path Routing
    struct mixer *mixer;
    struct audio_route *aroute;
    char *xml_path;

    // Mixer Update Thread
    pthread_rwlock_t mixer_update_lock;
    pthread_t        mixer_update_thread;

    audio_usage   active_playback_ausage;
    device_type   active_playback_device;
    modifier_type active_playback_modifier;

    audio_usage   active_capture_ausage;
    device_type   active_capture_device;
    modifier_type active_capture_modifier;

    // Primary Output Stream Proxy
    struct audio_proxy_stream *primary_out;

    /* Device Configuration */
    int num_earpiece;
    int num_speaker;
    int num_proximity;

    /* BuiltIn MIC Characteristics Map */
    int num_mic;
    struct audio_microphone_characteristic_t mic_info[AUDIO_MICROPHONE_MAX_COUNT];
    /* Bluetooth Configuration */
#ifdef SUPPORT_BTA2DP_OFFLOAD
    pthread_mutex_t a2dp_lock;
    bool     support_bta2dp;
    bool     a2dp_out_enabled;
    bool     a2dp_suspend;
    uint32_t a2dp_delay;
    uint32_t a2dp_default_delay;
#endif
    struct pcm *mute_playback;

    /* FM Radio Configuration */
    bool fm_internal;
    bool fm_external;

    struct pcm *fm_playback;     // FM PCM Playback from A-Box
    struct pcm *fm_capture;      // FM PCM Capture to A-Box

#ifdef SUPPORT_USB_OFFLOAD
    /* USB Configuration */
    bool is_usb_single_clksrc;   // USB device clock source info
    void *usb_aproxy;

    // PCM Devices for USB Audio
    bool support_usb_out_loopback;
    struct pcm *usb_out_loopback;
    bool support_usb_in_loopback;
    struct pcm *usb_in_loopback;
#endif

    /* PCM Devices for Voice Call */
    struct pcm *call_rx;    // CP to Output Devices
    struct pcm *call_tx;    // Input Devices to CP
    struct pcm *call_tx_direct;    // Direct routing for Input Devices

    // Call State
    bool call_state;
    struct call_param call_param_idx;

    /* Audio Mode */
    int audio_mode;
    bool skip_internalpath;     // flag to skip internal pcm close/re-open

    // Voice WakeUp
#ifdef SUPPORT_STHAL_INTERFACE
    /* SoundTrigger library interface */
    void *sound_trigger_lib;
    int (*sound_trigger_open_for_streaming)();
    size_t (*sound_trigger_read_samples)(int, void*, size_t);
    int (*sound_trigger_close_for_streaming)(int);
    int (*sound_trigger_open_recording)();
    size_t (*sound_trigger_read_recording_samples)(void*, size_t);
    int (*sound_trigger_close_recording)();

    int (*sound_trigger_headset_status)(int);
    int (*sound_trigger_voicecall_status)(int);
    int (*sound_trigger_notify_ahal_record_status)(int);

    int sthal_state;
#endif

    void *offload_effect_lib;
    void (*offload_effect_lib_update)(struct mixer *, int);

    bool support_dualspk;  //Dual Speaker
    bool spk_ampL_powerOn; //Dual Speaker
    bool incallmusic_rcv;
#ifdef SEC_AUDIO_DUMP
    int input_cnt;
#endif
    bool is_dp_connected;
};


#define MIXER_UPDATE_TIMEOUT    5  // 5 seconds

#define STR(s) #s
#define XSTR(s) STR(s)

#ifdef SUPPORT_STHAL_INTERFACE
#define SOUND_TRIGGER_HAL_LIBRARY_PATH "sound_trigger.primary.%s.so"
#endif

#if SEC_AUDIO_MULTI_MIC >= 3
#define VC_MULTI_MIC_CAPTURE_CHANNELS 4
#endif

#endif /* AUDIO_PROXY_H */
