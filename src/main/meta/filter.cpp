/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-filter
 * Created on: 16 июн. 2023 г.
 *
 * lsp-plugins-filter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-filter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-filter. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/filter.h>

#define LSP_PLUGINS_FILTER_VERSION_MAJOR         1
#define LSP_PLUGINS_FILTER_VERSION_MINOR         0
#define LSP_PLUGINS_FILTER_VERSION_MICRO         6

#define LSP_PLUGINS_FILTER_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_FILTER_VERSION_MAJOR, \
        LSP_PLUGINS_FILTER_VERSION_MINOR, \
        LSP_PLUGINS_FILTER_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Parametric Equalizer
        static const int plugin_classes[]           = { C_PARA_EQ, -1 };
        static const int clap_features_mono[]       = { CF_AUDIO_EFFECT, CF_EQUALIZER, CF_MONO, -1 };
        static const int clap_features_stereo[]     = { CF_AUDIO_EFFECT, CF_EQUALIZER, CF_STEREO, -1 };

        static const port_item_t filter_slopes[] =
        {
            { "x1",             "filter.slope.x1" },
            { "x2",             "filter.slope.x2" },
            { "x3",             "filter.slope.x3" },
            { "x4",             "filter.slope.x4" },
            { "x6",             "filter.slope.x6" },
            { "x8",             "filter.slope.x8" },
            { "x12",            "filter.slope.x12" },
            { "x16",            "filter.slope.x16" },
            { NULL, NULL }
        };

        static const port_item_t equalizer_eq_modes[] =
        {
            { "IIR",            "filter.type.iir" },
            { "FIR",            "filter.type.fir" },
            { "FFT",            "filter.type.fft" },
            { "SPM",            "filter.type.spm" },
            { NULL, NULL }
        };

        static const port_item_t filter_types[] =
        {
            { "Lo-pass",        "filter.flt.lopass" },
            { "Hi-pass",        "filter.flt.hipass" },
            { "Lo-shelf",       "filter.flt.loshelf" },
            { "Hi-shelf",       "filter.flt.hishelf" },
            { "Bell",           "filter.flt.bell" },
            { "Bandpass",       "filter.flt.bandpass" },
            { "Notch",          "filter.flt.notch" },
            { "Resonance",      "filter.flt.resonance" },
            { "Ladder-pass",    "filter.flt.ladpass" },
            { "Ladder-rej",     "filter.flt.ladrej" },
            { "Allpass",        "filter.flt.allpass" },
            { NULL, NULL }
        };

        static const port_item_t filter_modes[] =
        {
            { "RLC (BT)",       "filter.mode.rlc_bt" },
            { "RLC (MT)",       "filter.mode.rlc_mt" },
            { "BWC (BT)",       "filter.mode.bwc_bt" },
            { "BWC (MT)",       "filter.mode.bwc_mt" },
            { "LRX (BT)",       "filter.mode.lrx_bt" },
            { "LRX (MT)",       "filter.mode.lrx_mt" },
            { "APO (DR)",       "filter.mode.apo_dr" },
            { NULL, NULL }
        };

        #define EQ_FILTER \
                COMBO("ft", "Filter type", 0, filter_types), \
                COMBO("fm", "Filter mode", 0, filter_modes), \
                COMBO("s", "Filter slope", 0, filter_slopes), \
                LOG_CONTROL("f", "Frequency", U_HZ, filter_metadata::FREQ), \
                CONTROL("w", "Filter Width", U_OCTAVES, filter_metadata::WIDTH), \
                { "g", "Gain", U_GAIN_AMP, R_CONTROL, F_LOG | F_UPPER | F_LOWER | F_STEP, GAIN_AMP_M_36_DB, GAIN_AMP_P_36_DB, GAIN_AMP_0_DB, 0.01, NULL, NULL }, \
                { "q", "Quality factor", U_NONE, R_CONTROL, F_UPPER | F_LOWER | F_STEP, 0.0f, 100.0f, 0.0f, 0.025f, NULL        }

        #define EQ_COMMON \
                BYPASS, \
                AMP_GAIN("g_in", "Input gain", filter_metadata::IN_GAIN_DFL, 10.0f), \
                AMP_GAIN("g_out", "Output gain", filter_metadata::OUT_GAIN_DFL, 10.0f), \
                COMBO("mode", "Equalizer mode", 0, equalizer_eq_modes), \
                LOG_CONTROL("react", "FFT reactivity", U_MSEC, filter_metadata::REACT_TIME), \
                AMP_GAIN("shift", "Shift gain", 1.0f, 100.0f), \
                LOG_CONTROL("zoom", "Graph zoom", U_GAIN_AMP, filter_metadata::ZOOM)

        #define EQ_MONO_PORTS \
                MESH("ag", "Amplitude graph", 2, filter_metadata::MESH_POINTS), \
                METER_GAIN("im", "Input signal meter", GAIN_AMP_P_12_DB), \
                METER_GAIN("sm", "Output signal meter", GAIN_AMP_P_12_DB)

        #define EQ_STEREO_PORTS \
                PAN_CTL("bal", "Output balance", 0.0f), \
                MESH("ag", "Amplitude graph", 2, filter_metadata::MESH_POINTS), \
                METER_GAIN("iml", "Input signal meter Left", GAIN_AMP_P_12_DB), \
                METER_GAIN("sml", "Output signal meter Left", GAIN_AMP_P_12_DB), \
                METER_GAIN("imr", "Input signal meter Right", GAIN_AMP_P_12_DB), \
                METER_GAIN("smr", "Output signal meter Right", GAIN_AMP_P_12_DB)

        #define CHANNEL_ANALYSIS(id, label) \
                SWITCH("ife" id, "Input FFT graph enable" label, 1.0f), \
                SWITCH("ofe" id, "Output FFT graph enable" label, 1.0f), \
                MESH("ifg" id, "Input FFT graph" label, 2, filter_metadata::MESH_POINTS + 2), \
                MESH("ofg" id, "Output FFT graph" label, 2, filter_metadata::MESH_POINTS)


        static const port_t filter_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            EQ_COMMON,
            CHANNEL_ANALYSIS("", " "),
            EQ_MONO_PORTS,
            EQ_FILTER,

            PORTS_END
        };

        static const port_t filter_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            EQ_COMMON,
            CHANNEL_ANALYSIS("_l", " Left"),
            CHANNEL_ANALYSIS("_r", " Right"),
            EQ_STEREO_PORTS,
            EQ_FILTER,

            PORTS_END
        };

        const meta::bundle_t filter_bundle =
        {
            "filter",
            "Filter",
            B_EQUALIZERS,
            "BN0h4WnNnW8",
            "This plugin allows you to process a specific part of your audio's frequency spectrum."
        };

        const meta::plugin_t filter_mono =
        {
            "Filter Mono",
            "Filter Mono",
            "Filter Mono",
            "FLTM",
            &developers::v_sadovnikov,
            "filter_mono",
            LSP_LV2_URI("filter_mono"),
            LSP_LV2UI_URI("filter_mono"),
            "fltm",
            LSP_VST3_UID("fltm    fltm"),
            LSP_VST3UI_UID("fltm    fltm"),
            LSP_LADSPA_FILTER_BASE + 0,
            LSP_LADSPA_URI("filter_mono"),
            LSP_CLAP_URI("filter_mono"),
            LSP_PLUGINS_FILTER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            filter_mono_ports,
            "equalizer/filter/filter.xml",
            NULL,
            mono_plugin_port_groups,
            &filter_bundle
        };

        const meta::plugin_t filter_stereo =
        {
            "Filter Stereo",
            "Filter Stereo",
            "Filter Stereo",
            "FLTS",
            &developers::v_sadovnikov,
            "filter_stereo",
            LSP_LV2_URI("filter_stereo"),
            LSP_LV2UI_URI("filter_stereo"),
            "flts",
            LSP_VST3_UID("flts    flts"),
            LSP_VST3UI_UID("flts    flts"),
            LSP_LADSPA_FILTER_BASE + 1,
            LSP_LADSPA_URI("filter_stereo"),
            LSP_CLAP_URI("filter_stereo"),
            LSP_PLUGINS_FILTER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            filter_stereo_ports,
            "equalizer/filter/filter.xml",
            NULL,
            stereo_plugin_port_groups,
            &filter_bundle
        };

    } /* namespace meta */
} /* namespace lsp */
