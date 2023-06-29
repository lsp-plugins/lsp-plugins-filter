/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-para-equalizer
 * Created on: 2 авг. 2021 г.
 *
 * lsp-plugins-para-equalizer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-para-equalizer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-para-equalizer. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/filter.h>

#define LSP_PLUGINS_FILTER_VERSION_MAJOR         1
#define LSP_PLUGINS_FILTER_VERSION_MINOR         0
#define LSP_PLUGINS_FILTER_VERSION_MICRO         0

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
            { "x1",             "eq.slope.x1" },
            { "x2",             "eq.slope.x2" },
            { "x3",             "eq.slope.x3" },
            { "x4",             "eq.slope.x4" },
            { NULL, NULL }
        };

        static const port_item_t equalizer_eq_modes[] =
        {
            { "IIR",            "eq.type.iir" },
            { "FIR",            "eq.type.fir" },
            { "FFT",            "eq.type.fft" },
            { "SPM",            "eq.type.spm" },
            { NULL, NULL }
        };

        static const port_item_t filter_types[] =
        {
            { "Off",            "eq.flt.off" },
            { "Bell",           "eq.flt.bell" },
            { "Hi-pass",        "eq.flt.hipass" },
            { "Hi-shelf",       "eq.flt.hishelf" },
            { "Lo-pass",        "eq.flt.lopass" },
            { "Lo-shelf",       "eq.flt.loshelf" },
            { "Notch",          "eq.flt.notch" },
            { "Resonance",      "eq.flt.resonance" },
            { "Allpass",        "eq.flt.allpass" },

            // Additional stuff
        #ifdef LSP_USE_EXPERIMENTAL
            { "Allpass2",       "eq.flt.allpass2" },
            { "Ladder-pass",    "eq.flt.ladpass" },
            { "Ladder-rej",     "eq.flt.ladrej" },
            { "Envelope",       "eq.flt.envelope" },
            { "Bandpass",       "eq.flt.bandpass" },
            { "LUFS",           "eq.flt.lufs" },
        #endif /* LSP_USE_EXPERIMENTAL */
            { NULL, NULL }
        };

        static const port_item_t filter_modes[] =
        {
            { "RLC (BT)",       "eq.mode.rlc_bt" },
            { "RLC (MT)",       "eq.mode.rlc_mt" },
            { "BWC (BT)",       "eq.mode.bwc_bt" },
            { "BWC (MT)",       "eq.mode.bwc_mt" },
            { "LRX (BT)",       "eq.mode.lrx_bt" },
            { "LRX (MT)",       "eq.mode.lrx_mt" },
            { "APO (DR)",       "eq.mode.apo_dr" },
            { NULL, NULL }
        };

        static const port_item_t equalizer_fft_mode[] =
        {
            { "Off",            "metering.fft.off" },
            { "Post-eq",        "metering.fft.post_eq" },
            { "Pre-eq",         "metering.fft.pre_eq" },
            { NULL, NULL }
        };

        #define EQ_FILTER(id, label, x, total, f) \
                COMBO("ft" id "_" #x, "Filter type " label #x, 0, filter_types), \
                COMBO("fm" id "_" #x, "Filter mode " label #x, 0, filter_modes), \
                COMBO("s" id "_" #x, "Filter slope " label #x, 0, filter_slopes), \
                LOG_CONTROL_DFL("f" id "_" #x, "Frequency " label #x, U_HZ, filter_metadata::FREQ, f), \
                { "g" id "_" #x, "Gain " label # x, U_GAIN_AMP, R_CONTROL, F_IN | F_LOG | F_UPPER | F_LOWER | F_STEP, GAIN_AMP_M_36_DB, GAIN_AMP_P_36_DB, GAIN_AMP_0_DB, 0.01, NULL, NULL }, \
                { "q" id "_" #x, "Quality factor " label #x, U_NONE, R_CONTROL, F_IN | F_UPPER | F_LOWER | F_STEP, 0.0f, 100.0f, 0.0f, 0.025f, NULL        }

        #define EQ_FILTER_MONO(x, total, f)     EQ_FILTER("", "", x, total, f)
        #define EQ_FILTER_STEREO(x, total, f)   EQ_FILTER("", "", x, total, f)

        #define EQ_COMMON \
                BYPASS, \
                AMP_GAIN("g_in", "Input gain", filter_metadata::IN_GAIN_DFL, 10.0f), \
                AMP_GAIN("g_out", "Output gain", filter_metadata::OUT_GAIN_DFL, 10.0f), \
                COMBO("mode", "Equalizer mode", 0, equalizer_eq_modes), \
                COMBO("fft", "FFT analysis", 0, equalizer_fft_mode), \
                LOG_CONTROL("react", "FFT reactivity", U_MSEC, filter_metadata::REACT_TIME), \
                AMP_GAIN("shift", "Shift gain", 1.0f, 100.0f), \
                LOG_CONTROL("zoom", "Graph zoom", U_GAIN_AMP, filter_metadata::ZOOM)

        #define EQ_MONO_PORTS \
                MESH("ag", "Amplitude graph", 2, filter_metadata::MESH_POINTS), \
                METER_GAIN("im", "Input signal meter", GAIN_AMP_P_12_DB), \
                METER_GAIN("sm", "Output signal meter", GAIN_AMP_P_12_DB), \
                MESH("fftg", "FFT graph", 2, filter_metadata::MESH_POINTS), \
                SWITCH("fftv", "FFT visibility", 1.0f)

        #define EQ_STEREO_PORTS \
                PAN_CTL("bal", "Output balance", 0.0f), \
                MESH("ag", "Amplitude graph", 2, filter_metadata::MESH_POINTS), \
                METER_GAIN("iml", "Input signal meter Left", GAIN_AMP_P_12_DB), \
                METER_GAIN("sml", "Output signal meter Left", GAIN_AMP_P_12_DB), \
                MESH("fftg_l", "FFT channel Left", 2, filter_metadata::MESH_POINTS), \
                SWITCH("fftv_l", "FFT visibility Left", 1.0f), \
                METER_GAIN("imr", "Input signal meter Right", GAIN_AMP_P_12_DB), \
                METER_GAIN("smr", "Output signal meter Right", GAIN_AMP_P_12_DB), \
                MESH("fftg_r", "FFT channel Right", 2, filter_metadata::MESH_POINTS), \
                SWITCH("fftv_r", "FFT visibility Right", 1.0f)

        static const port_t filter_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            EQ_COMMON,
            EQ_MONO_PORTS,
            EQ_FILTER_MONO(0, 1, 10000.0f),

            PORTS_END
        };

        static const port_t filter_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            EQ_COMMON,
            EQ_STEREO_PORTS,
            EQ_FILTER_STEREO(0, 1, 10000.0f),

            PORTS_END
        };

        const meta::bundle_t filter_bundle =
        {
            "filter",
            "Filter",
            B_EQUALIZERS,
            "TfpJPsiouuU",
            "This plugin allows you to process a specific part of your audio's frequency spectrum."
        };

        const meta::plugin_t filter_mono =
        {
            "Filter Mono",
            "Filter Mono",
            "FLTM",
            &developers::v_sadovnikov,
            "filter_mono",
            LSP_LV2_URI("filter_mono"),
            LSP_LV2UI_URI("filter_mono"),
            "dh3y",
            LSP_LADSPA_FILTER_BASE + 0,
            LSP_LADSPA_URI("filter_mono"),
            LSP_CLAP_URI("filter_mono"),
            LSP_PLUGINS_FILTER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            filter_mono_ports,
            "equalizer/filter/mono.xml",
            "equalizer/filter/mono",
            mono_plugin_port_groups,
            &filter_bundle
        };

        const meta::plugin_t filter_stereo =
        {
            "Filter Stereo",
            "Filter Stereo",
            "FLTS",
            &developers::v_sadovnikov,
            "filter_stereo",
            LSP_LV2_URI("filter_stereo"),
            LSP_LV2UI_URI("filter_stereo"),
            "a5er",
            LSP_LADSPA_FILTER_BASE + 1,
            LSP_LADSPA_URI("filter_stereo"),
            LSP_CLAP_URI("filter_stereo"),
            LSP_PLUGINS_FILTER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            filter_stereo_ports,
            "equalizer/filter/stereo.xml",
            "equalizer/filter/stereo",
            stereo_plugin_port_groups,
            &filter_bundle
        };

    } /* namespace meta */
} /* namespace lsp */
