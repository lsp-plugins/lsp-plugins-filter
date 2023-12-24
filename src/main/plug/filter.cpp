/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
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

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/stdlib/math.h>

#include <lsp-plug.in/shared/debug.h>
#include <lsp-plug.in/shared/id_colors.h>

#include <private/plugins/filter.h>

#define EQ_BUFFER_SIZE          0x400U
#define EQ_RANK                 12

namespace lsp
{
    namespace plugins
    {
        //-------------------------------------------------------------------------
        // Plugin factory
        typedef struct plugin_settings_t
        {
            const meta::plugin_t   *metadata;
            uint8_t                 channels;
            uint8_t                 mode;
        } plugin_settings_t;

        static const meta::plugin_t *plugins[] =
        {
            &meta::filter_mono,
            &meta::filter_stereo
        };

        static const plugin_settings_t plugin_settings[] =
        {
            { &meta::filter_mono,   1, filter::EQ_MONO         },
            { &meta::filter_stereo, 1, filter::EQ_STEREO       },

            { NULL, 0, false }
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            for (const plugin_settings_t *s = plugin_settings; s->metadata != NULL; ++s)
                if (s->metadata == meta)
                    return new filter(s->metadata, s->mode);
            return NULL;
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //-------------------------------------------------------------------------
        filter::filter(const meta::plugin_t *metadata, size_t mode): plug::Module(metadata)
        {
            nMode           = mode;
            vChannels       = NULL;
            vFreqs          = NULL;
            vIndexes        = NULL;
            fGainIn         = 1.0f;
            fZoom           = 1.0f;
            bSmoothMode     = false;
            pIDisplay       = NULL;

            pBypass         = NULL;
            pGainIn         = NULL;
            pGainOut        = NULL;
            pReactivity     = NULL;
            pShiftGain      = NULL;
            pZoom           = NULL;
            pEqMode         = NULL;
            pBalance        = NULL;
        }

        filter::~filter()
        {
            do_destroy();
        }

        inline void filter::decode_filter(size_t *ftype, size_t *slope, size_t mode)
        {
            #define EQF(x) meta::filter_metadata::EQF_ ## x
            #define EQS(k, t, ks) case meta::filter_metadata::EFM_ ## k:    \
                    *ftype = dspu::t; \
                    *slope = ks * *slope; \
                    return;
            #define EQDFL  default: \
                    *ftype = dspu::FLT_NONE; \
                    *slope = 1; \
                    return;

            switch (*ftype)
            {
                case EQF(BELL):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_BELL, 1)
                        EQS(RLC_MT, FLT_MT_RLC_BELL, 1)
                        EQS(BWC_BT, FLT_BT_BWC_BELL, 1)
                        EQS(BWC_MT, FLT_MT_BWC_BELL, 1)
                        EQS(LRX_BT, FLT_BT_LRX_BELL, 1)
                        EQS(LRX_MT, FLT_MT_LRX_BELL, 1)
                        EQS(APO_DR, FLT_DR_APO_PEAKING, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(HIPASS):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_HIPASS, 2)
                        EQS(RLC_MT, FLT_MT_RLC_HIPASS, 2)
                        EQS(BWC_BT, FLT_BT_BWC_HIPASS, 2)
                        EQS(BWC_MT, FLT_MT_BWC_HIPASS, 2)
                        EQS(LRX_BT, FLT_BT_LRX_HIPASS, 1)
                        EQS(LRX_MT, FLT_MT_LRX_HIPASS, 1)
                        EQS(APO_DR, FLT_DR_APO_HIPASS, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(HISHELF):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_HISHELF, 1)
                        EQS(RLC_MT, FLT_MT_RLC_HISHELF, 1)
                        EQS(BWC_BT, FLT_BT_BWC_HISHELF, 1)
                        EQS(BWC_MT, FLT_MT_BWC_HISHELF, 1)
                        EQS(LRX_BT, FLT_BT_LRX_HISHELF, 1)
                        EQS(LRX_MT, FLT_MT_LRX_HISHELF, 1)
                        EQS(APO_DR, FLT_DR_APO_HISHELF, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(LOPASS):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_LOPASS, 2)
                        EQS(RLC_MT, FLT_MT_RLC_LOPASS, 2)
                        EQS(BWC_BT, FLT_BT_BWC_LOPASS, 2)
                        EQS(BWC_MT, FLT_MT_BWC_LOPASS, 2)
                        EQS(LRX_BT, FLT_BT_LRX_LOPASS, 1)
                        EQS(LRX_MT, FLT_MT_LRX_LOPASS, 1)
                        EQS(APO_DR, FLT_DR_APO_LOPASS, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(LOSHELF):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_LOSHELF, 1)
                        EQS(RLC_MT, FLT_MT_RLC_LOSHELF, 1)
                        EQS(BWC_BT, FLT_BT_BWC_LOSHELF, 1)
                        EQS(BWC_MT, FLT_MT_BWC_LOSHELF, 1)
                        EQS(LRX_BT, FLT_BT_LRX_LOSHELF, 1)
                        EQS(LRX_MT, FLT_MT_LRX_LOSHELF, 1)
                        EQS(APO_DR, FLT_DR_APO_LOSHELF, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(NOTCH):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_NOTCH, 1)
                        EQS(RLC_MT, FLT_MT_RLC_NOTCH, 1)
                        EQS(BWC_BT, FLT_BT_RLC_NOTCH, 1)
                        EQS(BWC_MT, FLT_MT_RLC_NOTCH, 1)
                        EQS(LRX_BT, FLT_BT_RLC_NOTCH, 1)
                        EQS(LRX_MT, FLT_MT_RLC_NOTCH, 1)
                        EQS(APO_DR, FLT_DR_APO_NOTCH, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(RESONANCE):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_RESONANCE, 1)
                        EQS(RLC_MT, FLT_MT_RLC_RESONANCE, 1)
                        EQS(BWC_BT, FLT_BT_RLC_RESONANCE, 1)
                        EQS(BWC_MT, FLT_MT_RLC_RESONANCE, 1)
                        EQS(LRX_BT, FLT_BT_RLC_RESONANCE, 1)
                        EQS(LRX_MT, FLT_MT_RLC_RESONANCE, 1)
                        EQS(APO_DR, FLT_DR_APO_PEAKING, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(LADDERPASS):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_LADDERPASS, 1)
                        EQS(RLC_MT, FLT_MT_RLC_LADDERPASS, 1)
                        EQS(BWC_BT, FLT_BT_BWC_LADDERPASS, 1)
                        EQS(BWC_MT, FLT_MT_BWC_LADDERPASS, 1)
                        EQS(LRX_BT, FLT_BT_LRX_LADDERPASS, 1)
                        EQS(LRX_MT, FLT_MT_LRX_LADDERPASS, 1)
                        EQS(APO_DR, FLT_DR_APO_LADDERPASS, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(LADDERREJ):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_LADDERREJ, 1)
                        EQS(RLC_MT, FLT_MT_RLC_LADDERREJ, 1)
                        EQS(BWC_BT, FLT_BT_BWC_LADDERREJ, 1)
                        EQS(BWC_MT, FLT_MT_BWC_LADDERREJ, 1)
                        EQS(LRX_BT, FLT_BT_LRX_LADDERREJ, 1)
                        EQS(LRX_MT, FLT_MT_LRX_LADDERREJ, 1)
                        EQS(APO_DR, FLT_DR_APO_LADDERREJ, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(BANDPASS):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_BANDPASS, 2)
                        EQS(RLC_MT, FLT_MT_RLC_BANDPASS, 2)
                        EQS(BWC_BT, FLT_BT_BWC_BANDPASS, 1)
                        EQS(BWC_MT, FLT_MT_BWC_BANDPASS, 1)
                        EQS(LRX_BT, FLT_BT_LRX_BANDPASS, 1)
                        EQS(LRX_MT, FLT_MT_LRX_BANDPASS, 1)
                        EQS(APO_DR, FLT_DR_APO_BANDPASS, 1)
                        EQDFL
                    }
                    break;
                }

                case EQF(ALLPASS):
                {
                    switch (mode)
                    {
                        EQS(RLC_BT, FLT_BT_RLC_ALLPASS, 1)
                        EQS(RLC_MT, FLT_BT_RLC_ALLPASS, 1)
                        EQS(BWC_BT, FLT_BT_BWC_ALLPASS, 2)
                        EQS(BWC_MT, FLT_BT_BWC_ALLPASS, 2)
                        EQS(LRX_BT, FLT_BT_LRX_ALLPASS, 1)
                        EQS(LRX_MT, FLT_BT_LRX_ALLPASS, 1)
                        EQS(APO_DR, FLT_DR_APO_ALLPASS, 1)
                        EQDFL
                    }
                    break;
                }

                EQDFL;
            }
            #undef EQDFL
            #undef EQS
            #undef EQF
        }

        size_t filter::decode_slope(size_t slope)
        {
            size_t arr[8]{1, 2, 3, 4, 6, 8, 12, 16};
            return arr[slope];
        }

        bool filter::filter_has_width(size_t type)
        {
            switch (type)
            {
                case dspu::FLT_BT_RLC_BANDPASS:
                case dspu::FLT_MT_RLC_BANDPASS:
                case dspu::FLT_BT_BWC_BANDPASS:
                case dspu::FLT_MT_BWC_BANDPASS:
                case dspu::FLT_BT_LRX_BANDPASS:
                case dspu::FLT_MT_LRX_BANDPASS:
                case dspu::FLT_BT_RLC_LADDERPASS:
                case dspu::FLT_MT_RLC_LADDERPASS:
                case dspu::FLT_BT_RLC_LADDERREJ:
                case dspu::FLT_MT_RLC_LADDERREJ:
                case dspu::FLT_BT_BWC_LADDERPASS:
                case dspu::FLT_MT_BWC_LADDERPASS:
                case dspu::FLT_BT_BWC_LADDERREJ:
                case dspu::FLT_MT_BWC_LADDERREJ:
                case dspu::FLT_BT_LRX_LADDERPASS:
                case dspu::FLT_MT_LRX_LADDERPASS:
                case dspu::FLT_BT_LRX_LADDERREJ:
                case dspu::FLT_MT_LRX_LADDERREJ:
                case dspu::FLT_DR_APO_LADDERPASS:
                case dspu::FLT_DR_APO_LADDERREJ:
                    return true;
            }

            return false;
        }

        inline bool filter::adjust_gain(size_t filter_type)
        {
            switch (filter_type)
            {
                case dspu::FLT_NONE:

                case dspu::FLT_BT_RLC_LOPASS:
                case dspu::FLT_MT_RLC_LOPASS:
                case dspu::FLT_BT_RLC_HIPASS:
                case dspu::FLT_MT_RLC_HIPASS:
                case dspu::FLT_BT_RLC_NOTCH:
                case dspu::FLT_MT_RLC_NOTCH:

                case dspu::FLT_BT_BWC_LOPASS:
                case dspu::FLT_MT_BWC_LOPASS:
                case dspu::FLT_BT_BWC_HIPASS:
                case dspu::FLT_MT_BWC_HIPASS:

                case dspu::FLT_BT_LRX_LOPASS:
                case dspu::FLT_MT_LRX_LOPASS:
                case dspu::FLT_BT_LRX_HIPASS:
                case dspu::FLT_MT_LRX_HIPASS:

                case dspu::FLT_BT_RLC_ALLPASS:
                case dspu::FLT_MT_RLC_ALLPASS:
                case dspu::FLT_BT_BWC_ALLPASS:
                case dspu::FLT_MT_BWC_ALLPASS:
                case dspu::FLT_BT_LRX_ALLPASS:
                case dspu::FLT_MT_LRX_ALLPASS:
                case dspu::FLT_DR_APO_ALLPASS:

                // Disable gain adjust for several APO filters, too
                case dspu::FLT_DR_APO_NOTCH:
                case dspu::FLT_DR_APO_LOPASS:
                case dspu::FLT_DR_APO_HIPASS:

                case dspu::FLT_BT_RLC_BANDPASS:
                case dspu::FLT_MT_RLC_BANDPASS:
                case dspu::FLT_BT_BWC_BANDPASS:
                case dspu::FLT_MT_BWC_BANDPASS:
                case dspu::FLT_BT_LRX_BANDPASS:
                case dspu::FLT_MT_LRX_BANDPASS:
                case dspu::FLT_DR_APO_BANDPASS:
                    return false;
                default:
                    break;
            }
            return true;
        }

        float   filter::calc_qfactor(float q, size_t type, size_t slope)
        {
            switch (type)
            {
                case dspu::FLT_BT_BWC_LOSHELF:
                case dspu::FLT_MT_BWC_LOSHELF:
                case dspu::FLT_BT_BWC_HISHELF:
                case dspu::FLT_MT_BWC_HISHELF:
                case dspu::FLT_BT_BWC_LADDERPASS:
                case dspu::FLT_MT_BWC_LADDERPASS:
                case dspu::FLT_BT_BWC_LADDERREJ:
                case dspu::FLT_MT_BWC_LADDERREJ:
                case dspu::FLT_BT_LRX_LOSHELF:
                case dspu::FLT_MT_LRX_LOSHELF:
                case dspu::FLT_BT_LRX_HISHELF:
                case dspu::FLT_MT_LRX_HISHELF:
                case dspu::FLT_BT_LRX_LADDERPASS:
                case dspu::FLT_MT_LRX_LADDERPASS:
                case dspu::FLT_BT_LRX_LADDERREJ:
                case dspu::FLT_MT_LRX_LADDERREJ:
                case dspu::FLT_BT_RLC_ALLPASS:
                case dspu::FLT_MT_RLC_ALLPASS:
                case dspu::FLT_BT_BWC_ALLPASS:
                case dspu::FLT_MT_BWC_ALLPASS:
                case dspu::FLT_BT_LRX_ALLPASS:
                case dspu::FLT_MT_LRX_ALLPASS:
                case dspu::FLT_DR_APO_ALLPASS:
                    return 0.0f;

                case dspu::FLT_BT_RLC_BELL:
                case dspu::FLT_MT_RLC_BELL:
                case dspu::FLT_BT_BWC_BELL:
                case dspu::FLT_MT_BWC_BELL:
                case dspu::FLT_BT_LRX_BELL:
                case dspu::FLT_MT_LRX_BELL:
                case dspu::FLT_BT_RLC_NOTCH:
                case dspu::FLT_MT_RLC_NOTCH:
                case dspu::FLT_DR_APO_LOPASS:
                case dspu::FLT_DR_APO_HIPASS:
                case dspu::FLT_DR_APO_BANDPASS:
                case dspu::FLT_DR_APO_NOTCH:
                case dspu::FLT_DR_APO_LOSHELF:
                case dspu::FLT_DR_APO_HISHELF:
                case dspu::FLT_DR_APO_LADDERPASS:
                case dspu::FLT_DR_APO_LADDERREJ:
                case dspu::FLT_BT_BWC_LOPASS:
                case dspu::FLT_MT_BWC_LOPASS:
                case dspu::FLT_BT_BWC_HIPASS:
                case dspu::FLT_MT_BWC_HIPASS:
                case dspu::FLT_BT_LRX_LOPASS:
                case dspu::FLT_MT_LRX_LOPASS:
                case dspu::FLT_BT_LRX_HIPASS:
                case dspu::FLT_MT_LRX_HIPASS:
                    return q;

                default:
                    break;
            }

            return q/slope;
        }

        inline dspu::equalizer_mode_t filter::get_eq_mode(ssize_t mode)
        {
            switch (mode)
            {
                case meta::filter_metadata::PEM_IIR: return dspu::EQM_IIR;
                case meta::filter_metadata::PEM_FIR: return dspu::EQM_FIR;
                case meta::filter_metadata::PEM_FFT: return dspu::EQM_FFT;
                case meta::filter_metadata::PEM_SPM: return dspu::EQM_SPM;
                default:
                    break;
            }
            return dspu::EQM_BYPASS;
        }

        void filter::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Pass wrapper
            plug::Module::init(wrapper, ports);

            // Determine number of channels
            size_t channels     = (nMode == EQ_MONO) ? 1 : 2;
            size_t max_latency  = 0;

            // Allocate channels
            vChannels           = new eq_channel_t[channels];
            if (vChannels == NULL)
                return;

            // Initialize global parameters
            fGainIn             = 1.0f;

            // Allocate indexes
            vIndexes            = new uint32_t[meta::filter_metadata::MESH_POINTS];
            if (vIndexes == NULL)
                return;

            // Calculate amount of bulk data to allocate
            size_t allocate     =
                meta::filter_metadata::MESH_POINTS + // vFreqs
                channels * (
                    EQ_BUFFER_SIZE + // vDryBuf
                    EQ_BUFFER_SIZE + // vBuffer
                    EQ_BUFFER_SIZE + // vAnalyzer
                    2 * meta::filter_metadata::MESH_POINTS +    // vTr
                    meta::filter_metadata::MESH_POINTS          // vTrMem
                );
            float *abuf         = new float[allocate];
            if (abuf == NULL)
                return;
            lsp_guard_assert(float *save   = abuf);

            // Clear all floating-point buffers
            dsp::fill_zero(abuf, allocate);

            // Frequency list buffer
            vFreqs              = advance_ptr<float>(abuf, meta::filter_metadata::MESH_POINTS);

            // Initialize each channel
            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c     = &vChannels[i];

                // Initialize equalizer
                c->sEqualizer.init(1, EQ_RANK);
                c->sEqualizer.set_smooth(true);
                max_latency         = lsp_max(max_latency, c->sEqualizer.max_latency());

                // Init filter parameters
                c->sOldFP.nType     = dspu::FLT_NONE;
                c->sOldFP.fFreq     = 0.0f;
                c->sOldFP.fFreq2    = 0.0f;
                c->sOldFP.fGain     = GAIN_AMP_0_DB;
                c->sOldFP.nSlope    = 0;
                c->sOldFP.fQuality  = 0.0f;

                c->sFP.nType        = dspu::FLT_NONE;
                c->sFP.fFreq        = 0.0f;
                c->sFP.fFreq2       = 0.0f;
                c->sFP.fGain        = GAIN_AMP_0_DB;
                c->sFP.nSlope       = 0;
                c->sFP.fQuality     = 0.0f;

                c->nLatency         = 0;
                c->fInGain          = 1.0f;
                c->fOutGain         = 1.0f;
                c->vDryBuf          = advance_ptr<float>(abuf, EQ_BUFFER_SIZE);
                c->vBuffer          = advance_ptr<float>(abuf, EQ_BUFFER_SIZE);
                c->vIn              = NULL;
                c->vOut             = NULL;
                c->vAnalyzer        = advance_ptr<float>(abuf, EQ_BUFFER_SIZE);
                c->vTr              = advance_ptr<float>(abuf, meta::filter_metadata::MESH_POINTS * 2);
                c->vTrMem           = advance_ptr<float>(abuf, meta::filter_metadata::MESH_POINTS);
                c->nSync            = CS_UPDATE;

                // Ports
                c->pType            = NULL;
                c->pMode            = NULL;
                c->pFreq            = NULL;
                c->pWidth           = NULL;
                c->pGain            = NULL;
                c->pQuality         = NULL;

                c->pIn              = NULL;
                c->pOut             = NULL;
                c->pInGain          = NULL;
                c->pTrAmp           = NULL;
                c->pFftInSwitch     = NULL;
                c->pFftOutSwitch    = NULL;
                c->pFftInMesh       = NULL;
                c->pFftOutMesh      = NULL;
                c->pInMeter         = NULL;
                c->pOutMeter        = NULL;
            }

            lsp_assert(abuf <= &save[allocate]);

            // Initialize latency compensation delay
            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c     = &vChannels[i];
                if (!c->sDryDelay.init(max_latency))
                    return;
            }

            // Bind ports
            size_t port_id          = 0;

            // Bind audio ports
            lsp_trace("Binding audio ports");
            for (size_t i=0; i<channels; ++i)
                vChannels[i].pIn        =   trace_port(ports[port_id++]);
            for (size_t i=0; i<channels; ++i)
                vChannels[i].pOut       =   trace_port(ports[port_id++]);

            // Bind common ports
            lsp_trace("Binding common ports");
            pBypass                 = trace_port(ports[port_id++]);
            pGainIn                 = trace_port(ports[port_id++]);
            pGainOut                = trace_port(ports[port_id++]);
            pEqMode                 = trace_port(ports[port_id++]);
            pReactivity             = trace_port(ports[port_id++]);
            pShiftGain              = trace_port(ports[port_id++]);
            pZoom                   = trace_port(ports[port_id++]);

            // Meters
            for (size_t i=0; i<channels; ++i)
            {
               eq_channel_t *c     = &vChannels[i];

               c->pFftInSwitch         = trace_port(ports[port_id++]);
               c->pFftOutSwitch        = trace_port(ports[port_id++]);
               c->pFftInMesh           = trace_port(ports[port_id++]);
               c->pFftOutMesh          = trace_port(ports[port_id++]);
            }

            // Balance
            if (channels > 1)
                pBalance                = trace_port(ports[port_id++]);

            for (size_t i=0; i<channels; ++i)
            {
                if ((nMode == EQ_STEREO) && (i > 0))
                {
                    vChannels[i].pTrAmp     = NULL;
                }
                else
                {
                    vChannels[i].pTrAmp     = trace_port(ports[port_id++]);
                }
                vChannels[i].pInMeter   =   trace_port(ports[port_id++]);
                vChannels[i].pOutMeter  =   trace_port(ports[port_id++]);
            }

            // Bind filters
            lsp_trace("Binding filter ports");

            for (size_t j=0; j<channels; ++j)
            {
                eq_channel_t *c     = &vChannels[j];

                if ((nMode == EQ_STEREO) && (j > 0))
                {
                    // 1 port controls 2 filters
                    eq_channel_t *sc    = &vChannels[0];
                    c->pType            = sc->pType;
                    c->pMode            = sc->pMode;
                    c->pSlope           = sc->pSlope;
                    c->pFreq            = sc->pFreq;
                    c->pWidth           = sc->pWidth;
                    c->pGain            = sc->pGain;
                    c->pQuality         = sc->pQuality;
                }
                else
                {
                    // 1 port controls 1 filter
                    c->pType        = trace_port(ports[port_id++]);
                    c->pMode        = trace_port(ports[port_id++]);
                    c->pSlope       = trace_port(ports[port_id++]);
                    c->pFreq        = trace_port(ports[port_id++]);
                    c->pWidth       = trace_port(ports[port_id++]);
                    c->pGain        = trace_port(ports[port_id++]);
                    c->pQuality     = trace_port(ports[port_id++]);
                }
            }
        }

        void filter::ui_activated()
        {
            size_t channels     = ((nMode == EQ_MONO) || (nMode == EQ_STEREO)) ? 1 : 2;
            for (size_t i=0; i<channels; ++i)
                vChannels[i].nSync = CS_UPDATE;

            pWrapper->request_settings_update();
        }

        void filter::ui_deactivated()
        {
            pWrapper->request_settings_update();
        }

        void filter::destroy()
        {
            Module::destroy();
            do_destroy();
        }

        void filter::do_destroy()
        {
            // Delete channels
            if (vChannels != NULL)
            {
                delete [] vChannels;
                vChannels = NULL;
            }

            if (vIndexes != NULL)
            {
                delete [] vIndexes;
                vIndexes    = NULL;
            }

            // Delete frequencies
            if (vFreqs != NULL)
            {
                delete [] vFreqs;
                vFreqs = NULL;
            }

            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay   = NULL;
            }

            // Destroy analyzer
            sAnalyzer.destroy();
        }

        void filter::update_settings()
        {
            // Check sample rate
            if (fSampleRate <= 0)
                return;

            // Update common settings
            if (pGainIn != NULL)
                fGainIn     = pGainIn->value();
            if (pZoom != NULL)
            {
                float zoom  = pZoom->value();
                if (zoom != fZoom)
                {
                    fZoom       = zoom;
                    pWrapper->query_display_draw();
                }
            }

            // Calculate balance
            float bal[2] = { 1.0f, 1.0f };
            if (pBalance != NULL)
            {
                float xbal      = pBalance->value();
                bal[0]          = (100.0f - xbal) * 0.01f;
                bal[1]          = (xbal + 100.0f) * 0.01f;
            }
            if (pGainOut != NULL)
            {
                float out_gain  = pGainOut->value();
                bal[0]         *= out_gain;
                bal[1]         *= out_gain;
            }


            size_t channels     = (nMode == EQ_MONO) ? 1 : 2;

            // Configure analyzer
            size_t n_an_channels = 0;
            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c     = &vChannels[i];
                bool in_fft         = c->pFftInSwitch->value() >= 0.5f;
                bool out_fft        = c->pFftOutSwitch->value() >= 0.5f;

                // channel:        0     1     2      3
                // designation: in_l out_l  in_r  out_r
                sAnalyzer.enable_channel(i*2, in_fft);
                sAnalyzer.enable_channel(i*2+1, out_fft);
                if ((in_fft) || (out_fft))
                    ++n_an_channels;
            }

            // Update reactivity
            sAnalyzer.set_activity(n_an_channels > 0);
            sAnalyzer.set_reactivity(pReactivity->value());

            // Update shift gain
            if (pShiftGain != NULL)
                sAnalyzer.set_shift(pShiftGain->value() * 100.0f);

            // Update equalizer mode
            dspu::equalizer_mode_t eq_mode  = get_eq_mode(pEqMode->value());
            bool bypass                     = pBypass->value() >= 0.5f;
            bool mode_changed               = false;
            bSmoothMode                     = false;

            // For each channel
            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c     = &vChannels[i];

                // Change the operating mode for the equalizer
                if (c->sEqualizer.mode() != eq_mode)
                {
                    c->sEqualizer.set_mode(eq_mode);
                    mode_changed        = true;
                }

                // Update settings
                if (c->sBypass.set_bypass(bypass))
                    pWrapper->query_display_draw();
                c->fOutGain         = bal[i];
                if (c->pInGain != NULL)
                    c->fInGain          = c->pInGain->value();

                // Update filter configuration
                c->sOldFP           = c->sFP;
                dspu::filter_params_t *fp = &c->sFP;
                dspu::filter_params_t *op = &c->sOldFP;

                // Compute filter params
                fp->nType           = c->pType->value();
                fp->nSlope          = decode_slope(c->pSlope->value());
                decode_filter(&fp->nType, &fp->nSlope, c->pMode->value());

                if (filter_has_width(fp->nType))
                {
                    float center = c->pFreq->value();
                    float k = powf(2, (c->pWidth->value()*0.5f));
                    fp->fFreq           = center/k;
                    fp->fFreq2          = center*k;
                }
                else
                {
                    fp->fFreq           = c->pFreq->value();
                    fp->fFreq2          = fp->fFreq;
                }
                fp->fGain           = (adjust_gain(fp->nType)) ? c->pGain->value() : 1.0f;
                fp->fQuality        = calc_qfactor(c->pQuality->value(), fp->nType, fp->nSlope);

                c->sEqualizer.limit_params(0, fp);
                bool type_changed   =
                    (fp->nType != op->nType) ||
                    (fp->nSlope != op->nSlope);
                bool param_changed  =
                    (fp->fGain != op->fGain) ||
                    (fp->fFreq != op->fFreq) ||
                    (fp->fFreq2 != op->fFreq2) ||
                    (fp->fQuality != op->fQuality);

                // Apply filter params if theey have changed
                if ((type_changed) || (param_changed))
                {
                    c->sEqualizer.set_params(0, fp);
                    c->nSync            = CS_UPDATE;

                    if (type_changed)
                        mode_changed    = true;
                    if (param_changed)
                        bSmoothMode     = true;
                }

            }

            // Do not enable smooth mode if significant changes have been applied
            if ((mode_changed) || (eq_mode != dspu::EQM_IIR))
                bSmoothMode             = false;

            // Update analyzer
            if (sAnalyzer.needs_reconfiguration())
            {
                sAnalyzer.reconfigure();
                sAnalyzer.get_frequencies(vFreqs, vIndexes, SPEC_FREQ_MIN, SPEC_FREQ_MAX, meta::filter_metadata::MESH_POINTS);
            }

            // Update latency
            size_t latency          = 0;
            for (size_t i=0; i<channels; ++i)
                latency                 = lsp_max(latency, vChannels[i].sEqualizer.get_latency());

            for (size_t i=0; i<channels; ++i)
            {
                vChannels[i].sDryDelay.set_delay(latency);
                sAnalyzer.set_channel_delay(i*2, latency);
            }
            set_latency(latency);
        }

        void filter::update_sample_rate(long sr)
        {
            size_t channels     = (nMode == EQ_MONO) ? 1 : 2;

            sAnalyzer.set_sample_rate(sr);
            size_t max_latency  = 1 << (meta::filter_metadata::FFT_RANK + 1);

            // Initialize channels
            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c     = &vChannels[i];
                c->sBypass.init(sr);
                c->sEqualizer.set_sample_rate(sr);
            }

            // Initialize analyzer
            if (!sAnalyzer.init(channels*2, meta::filter_metadata::FFT_RANK,
                                sr, meta::filter_metadata::REFRESH_RATE,
                                max_latency))
                return;

            sAnalyzer.set_sample_rate(sr);
            sAnalyzer.set_rank(meta::filter_metadata::FFT_RANK);
            sAnalyzer.set_activity(false);
            sAnalyzer.set_envelope(meta::filter_metadata::FFT_ENVELOPE);
            sAnalyzer.set_window(meta::filter_metadata::FFT_WINDOW);
            sAnalyzer.set_rate(meta::filter_metadata::REFRESH_RATE);
        }

        void filter::perform_analysis(size_t samples)
        {
            // Do not do anything if analyzer is inactive
            if (!sAnalyzer.activity())
                return;

            // Prepare processing
            size_t channels     = (nMode == EQ_MONO) ? 1 : 2;

            const float *bufs[4] = { NULL, NULL, NULL, NULL };
            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c         = &vChannels[i];
                bufs[i*2]               = c->vAnalyzer;
                bufs[i*2+1]             = c->vBuffer;
            }

            // Perform FFT analysis
            sAnalyzer.process(bufs, samples);
        }

        void filter::process_channel(eq_channel_t *c, size_t start, size_t samples)
        {
            // Process the signal by the equalizer
            if (bSmoothMode)
            {
                float den   = 1.0f / samples;

                // In smooth mode, we need to update filter parameters for each sample
                for (size_t offset=0; offset<samples; ++offset)
                {
                    // Tune the filters
                    float k                     = float(start + offset) * den;

                    dspu::filter_params_t fp;

                    fp.nType                    = c->sFP.nType;
                    fp.fFreq                    = c->sOldFP.fFreq * expf(logf(c->sFP.fFreq/c->sOldFP.fFreq)*k);
                    fp.fFreq2                   = c->sOldFP.fFreq2 * expf(logf(c->sFP.fFreq2/c->sOldFP.fFreq2)*k);
                    fp.nSlope                   = c->sFP.nSlope;
                    fp.fGain                    = c->sOldFP.fGain * expf(logf(c->sFP.fGain/c->sOldFP.fGain)*k);
                    fp.fQuality                 = c->sOldFP.fQuality + (c->sFP.fQuality - c->sOldFP.fQuality)*k;

                    c->sEqualizer.set_params(0, &fp);

                    // Apply processing
                    c->sEqualizer.process(&c->vBuffer[offset], &c->vBuffer[offset], 1);
                }
            }
            else
                c->sEqualizer.process(c->vBuffer, c->vBuffer, samples);

            if (c->fInGain != 1.0f)
                dsp::mul_k2(c->vBuffer, c->fInGain, samples);
        }

        void filter::process(size_t samples)
        {
            size_t channels     = (nMode == EQ_MONO) ? 1 : 2;

            // Initialize buffer pointers
            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c     = &vChannels[i];
                c->vIn              = c->pIn->buffer<float>();
                c->vOut             = c->pOut->buffer<float>();
            }

            for (size_t offset = 0; offset < samples; )
            {
                // Determine buffer size for processing
                size_t to_process   = lsp_min(samples-offset, EQ_BUFFER_SIZE);

                // Store unprocessed data
                for (size_t i=0; i<channels; ++i)
                {
                    eq_channel_t *c     = &vChannels[i];
                    c->sDryDelay.process(c->vDryBuf, c->vIn, to_process);
                }

                if (nMode == EQ_MONO)
                {
                    vChannels[0].pInMeter->set_value(dsp::abs_max(vChannels[0].vIn, to_process));
                    if (fGainIn != 1.0f)
                        dsp::mul_k3(vChannels[0].vBuffer, vChannels[0].vIn, fGainIn, to_process);
                    else
                        dsp::copy(vChannels[0].vBuffer, vChannels[0].vIn, to_process);
                }
                else
                {
                    vChannels[0].pInMeter->set_value(dsp::abs_max(vChannels[0].vIn, to_process));
                    vChannels[1].pInMeter->set_value(dsp::abs_max(vChannels[1].vIn, to_process));
                    if (fGainIn != 1.0f)
                    {
                        dsp::mul_k3(vChannels[0].vBuffer, vChannels[0].vIn, fGainIn, to_process);
                        dsp::mul_k3(vChannels[1].vBuffer, vChannels[1].vIn, fGainIn, to_process);
                    }
                    else
                    {
                        dsp::copy(vChannels[0].vBuffer, vChannels[0].vIn, to_process);
                        dsp::copy(vChannels[1].vBuffer, vChannels[1].vIn, to_process);
                    }
                }

                // Store data for analysis
                for (size_t i=0; i<channels; ++i)
                {
                    eq_channel_t *c     = &vChannels[i];
                    if (sAnalyzer.channel_active(i*2))
                        dsp::copy(c->vAnalyzer, c->vBuffer, to_process);
                }

                // Process each channel individually
                for (size_t i=0; i<channels; ++i)
                    process_channel(&vChannels[i], offset, to_process);

                // Call analyzer
                perform_analysis(to_process);

                // Process data via bypass
                for (size_t i=0; i<channels; ++i)
                {
                    eq_channel_t *c     = &vChannels[i];

                    // Apply output gain
                    if (c->fOutGain != 1.0f)
                        dsp::mul_k2(c->vBuffer, c->fOutGain, to_process);

                    // Do output metering
                    if (c->pOutMeter != NULL)
                        c->pOutMeter->set_value(dsp::abs_max(c->vBuffer, to_process));

                    // Process via bypass
                    c->sBypass.process(c->vOut, c->vDryBuf, c->vBuffer, to_process);

                    c->vIn             += to_process;
                    c->vOut            += to_process;
                }

                // Update offset
                offset             += to_process;
            } // for offset

            // Output FFT curves for each channel and report latency
            size_t latency          = 0;

            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c     = &vChannels[i];

                if (latency < c->sEqualizer.get_latency())
                    latency         = c->sEqualizer.get_latency();

                // Input FFT mesh
                plug::mesh_t *mesh          = c->pFftInMesh->buffer<plug::mesh_t>();
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    // Add extra points
                    mesh->pvData[0][0] = SPEC_FREQ_MIN * 0.5f;
                    mesh->pvData[0][meta::filter_metadata::MESH_POINTS+1] = SPEC_FREQ_MAX * 2.0f;
                    mesh->pvData[1][0] = 0.0f;
                    mesh->pvData[1][meta::filter_metadata::MESH_POINTS+1] = 0.0f;

                    // Copy frequency points
                    dsp::copy(&mesh->pvData[0][1], vFreqs, meta::filter_metadata::MESH_POINTS);
                    sAnalyzer.get_spectrum(i*2, &mesh->pvData[1][1], vIndexes, meta::filter_metadata::MESH_POINTS);

                    // Mark mesh containing data
                    mesh->data(2, meta::filter_metadata::MESH_POINTS+2);
                }

                // Output FFT mesh
                mesh                        = c->pFftOutMesh->buffer<plug::mesh_t>();
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    // Copy frequency points
                    dsp::copy(mesh->pvData[0], vFreqs, meta::filter_metadata::MESH_POINTS);
                    sAnalyzer.get_spectrum(i*2+1, mesh->pvData[1], vIndexes, meta::filter_metadata::MESH_POINTS);

                    // Mark mesh containing data
                    mesh->data(2, meta::filter_metadata::MESH_POINTS);
                }
            }

            set_latency(latency);

            // For Mono and Stereo channels only the first channel should be processed
            if (nMode == EQ_STEREO)
                channels        = 1;

            // Sync meshes
            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c     = &vChannels[i];
                if (c->pTrAmp == NULL)
                    continue;

                // Synchronize main transfer function of the channel
                if (c->nSync & CS_UPDATE)
                {
                    c->sEqualizer.freq_chart(c->vTr, vFreqs, meta::filter_metadata::MESH_POINTS);
                    dsp::pcomplex_mod(c->vTrMem, c->vTr, meta::filter_metadata::MESH_POINTS);
                    c->nSync    = CS_SYNC_AMP;
                }

                // Output amplification curve
                if (c->nSync & CS_SYNC_AMP)
                {
                    // Sync mesh
                    plug::mesh_t *mesh  = c->pTrAmp->buffer<plug::mesh_t>();
                    if ((mesh != NULL) && (mesh->isEmpty()))
                    {
                        dsp::copy(mesh->pvData[0], vFreqs, meta::filter_metadata::MESH_POINTS);
                        dsp::copy(mesh->pvData[1], c->vTrMem, meta::filter_metadata::MESH_POINTS);
                        mesh->data(2, meta::filter_metadata::MESH_POINTS);

                        c->nSync           &= ~CS_SYNC_AMP;
                    }

                    // Request for redraw
                    if (pWrapper != NULL)
                        pWrapper->query_display_draw();
                }
            }

            // Reset smooth mode
            if (bSmoothMode)
            {
                // Apply actual settings of equalizer at the end
                for (size_t i=0; i<channels; ++i)
                {
                    eq_channel_t *c     = &vChannels[i];
                    c->sEqualizer.set_params(0, &c->sFP);
                }

                bSmoothMode     = false;
            }
        }

        bool filter::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > (M_RGOLD_RATIO * width))
                height  = M_RGOLD_RATIO * width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width   = cv->width();
            height  = cv->height();

            // Clear background
            bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            // Draw axis
            cv->set_line_width(1.0);

            float zx    = 1.0f/SPEC_FREQ_MIN;
            float zy    = fZoom/GAIN_AMP_M_48_DB;
            float dx    = width/(logf(SPEC_FREQ_MAX)-logf(SPEC_FREQ_MIN));
            float dy    = height/(logf(GAIN_AMP_M_48_DB/fZoom)-logf(GAIN_AMP_P_48_DB*fZoom));

            // Draw vertical lines
            cv->set_color_rgb(CV_YELLOW, 0.5f);
            for (float i=100.0f; i<SPEC_FREQ_MAX; i *= 10.0f)
            {
                float ax = dx*(logf(i*zx));
                cv->line(ax, 0, ax, height);
            }

            // Draw horizontal lines
            cv->set_color_rgb(CV_WHITE, 0.5f);
            for (float i=GAIN_AMP_M_48_DB; i<GAIN_AMP_P_48_DB; i *= GAIN_AMP_P_12_DB)
            {
                float ay = height + dy*(logf(i*zy));
                cv->line(0, ay, width, ay);
            }

            // Allocate buffer: f, x, y, amp
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width+2);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            // Initialize mesh
            b->v[0][0]          = SPEC_FREQ_MIN*0.5f;
            b->v[0][width+1]    = SPEC_FREQ_MAX*2.0f;
            b->v[3][0]          = 1.0f;
            b->v[3][width+1]    = 1.0f;

            size_t channels = ((nMode == EQ_MONO) || (nMode == EQ_STEREO)) ? 1 : 2;
            bool aa = cv->set_anti_aliasing(true);
            cv->set_line_width(2);

            for (size_t i=0; i<channels; ++i)
            {
                eq_channel_t *c = &vChannels[i];

                for (size_t j=0; j<width; ++j)
                {
                    size_t k        = (j*meta::filter_metadata::MESH_POINTS)/width;
                    b->v[0][j+1]    = vFreqs[k];
                    b->v[3][j+1]    = c->vTrMem[k];
                }

                dsp::fill(b->v[1], 0.0f, width+2);
                dsp::fill(b->v[2], height, width+2);
                dsp::axis_apply_log1(b->v[1], b->v[0], zx, dx, width+2);
                dsp::axis_apply_log1(b->v[2], b->v[3], zy, dy, width+2);

                // Draw mesh
                uint32_t color = (bypassing || !(active())) ? CV_SILVER : CV_MIDDLE_CHANNEL;
                Color stroke(color), fill(color, 0.5f);
                cv->draw_poly(b->v[1], b->v[2], width+2, stroke, fill);
            }
            cv->set_anti_aliasing(aa);

            return true;
        }

        void filter::dump_filter_params(dspu::IStateDumper *v, const char *id, const dspu::filter_params_t *fp)
        {
            v->begin_object(id, fp, sizeof(*fp));
            {
                v->write("nType", fp->nType);
                v->write("fFreq", fp->fFreq);
                v->write("fFreq2", fp->fFreq2);
                v->write("fGain", fp->fGain);
                v->write("nSlope", fp->nSlope);
                v->write("fQuality", fp->fQuality);
            }
            v->end_object();
        }

        void filter::dump_channel(dspu::IStateDumper *v, const eq_channel_t *c) const
        {
            v->begin_object(c, sizeof(eq_channel_t));
            {
                v->write_object("sEqualizer", &c->sEqualizer);
                v->write_object("sBypass", &c->sBypass);
                v->write_object("sDryDelay", &c->sDryDelay);

                dump_filter_params(v, "sOldFP", &c->sOldFP);
                dump_filter_params(v, "sFP", &c->sFP);

                v->write("nLatency", c->nLatency);
                v->write("fInGain", c->fInGain);
                v->write("fOutGain", c->fOutGain);
                v->write("vDryBuf", c->vDryBuf);
                v->write("vBuffer", c->vBuffer);
                v->write("vIn", c->vIn);
                v->write("vOut", c->vOut);
                v->write("vAnalyzer", c->vAnalyzer);
                v->write("vTr", c->vTr);
                v->write("vTrMem", c->vTrMem);
                v->write("nSync", c->nSync);

                v->write("pType", c->pType);
                v->write("pMode", c->pMode);
                v->write("pFreq", c->pFreq);
                v->write("pSlope", c->pSlope);
                v->write("pGain", c->pGain);
                v->write("pQuality", c->pQuality);

                v->write("pIn", c->pIn);
                v->write("pOut", c->pOut);
                v->write("pInGain", c->pInGain);
                v->write("pTrAmp", c->pTrAmp);
                v->write("pFftInSwitch", c->pFftInSwitch);
                v->write("pFftOutSwitch", c->pFftOutSwitch);
                v->write("pFftInMesh", c->pFftInMesh);
                v->write("pFftOutMesh", c->pFftOutMesh);
                v->write("pInMeter", c->pInMeter);
                v->write("pOutMeter", c->pOutMeter);
            }
            v->end_object();
        }

        void filter::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            size_t channels     = (nMode == EQ_MONO) ? 1 : 2;

            v->write_object("sAnalyzer", &sAnalyzer);
            v->write("nMode", nMode);
            v->begin_array("vChannels", vChannels, channels);
            {
                for (size_t i=0; i<channels; ++i)
                    dump_channel(v, &vChannels[i]);
            }
            v->end_array();
            v->write("vFreqs", vFreqs);
            v->write("vIndexes", vIndexes);
            v->write("fGainIn", fGainIn);
            v->write("fZoom", fZoom);
            v->write("bSmoothMode", bSmoothMode);
            v->write_object("pIDisplay", pIDisplay);
            v->write("pBypass", pBypass);
            v->write("pGainIn", pGainIn);
            v->write("pGainOut", pGainOut);
            v->write("pReactivity", pReactivity);
            v->write("pShiftGain", pShiftGain);
            v->write("pZoom", pZoom);
            v->write("pEqMode", pEqMode);
            v->write("pBalance", pBalance);
        }

    } /* namespace plugins */
} /* namespace lsp */
