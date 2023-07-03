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

#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/fmt/RoomEQWizard.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/stdlib/string.h>
#include <lsp-plug.in/tk/tk.h>
#include <private/meta/filter.h>

#include <private/ui/filter.h>

namespace lsp
{
    namespace plugins
    {
        //---------------------------------------------------------------------
        // Plugin UI factory
        static const meta::plugin_t *plugin_uis[] =
        {
            &meta::filter_mono,
            &meta::filter_stereo
        };

        static ui::Module *ui_factory(const meta::plugin_t *meta)
        {
            return new filter_ui(meta);
        }

        static ui::Factory factory(ui_factory, plugin_uis, 2);


        //---------------------------------------------------------------------
        static const char *note_names[] =
        {
            "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"
        };

        template <class T>
        T *filter_ui::filter_widget(const char *widget_id)
        {
            return pWrapper->controller()->widgets()->get<T>(widget_id);
        }

        ui::IPort *filter_ui::find_port(const char *port_id)
        {
            return pWrapper->port(port_id);
        }

        filter_ui::filter_ui(const meta::plugin_t *meta): ui::Module(meta)
        {
            pType       = NULL;
            pFreq       = NULL;
            wNote       = NULL;
        }

        filter_ui::~filter_ui()
        {

        }

        void filter_ui::update_filter_note_text()
        {
            // Check that we have the widget to display
            if (wNote == NULL)
                return;

            // Get the frequency
            float freq = (pFreq != NULL) ? pFreq->value() : -1.0f;
            if (freq < 0.0f)
            {
               // wNote->visibility()->set(false);
                return;
            }

            // Check that filter is enabled
            ssize_t type = (pType != NULL) ? ssize_t(pType->value()) : -1;
            if (type < 0)
            {
                return;
            }

            // Update the note name displayed in the text
            {
                // Fill the parameters
                expr::Parameters params;
                tk::prop::String lc_string;
                LSPString text;
                lc_string.bind(wNote->style(), pDisplay->dictionary());

                // Frequency
                text.fmt_ascii("%.2f", freq);
                params.set_string("frequency", &text);

                // Process filter type
                text.fmt_ascii("lists.%s", pType->metadata()->items[type].lc_key);
                lc_string.set(&text);
                lc_string.format(&text);
                params.set_string("filter_type", &text);

                // Process filter note
                float note_full = dspu::frequency_to_note(freq);
                if (note_full != dspu::NOTE_OUT_OF_RANGE)
                {
                    note_full += 0.5f;
                    ssize_t note_number = ssize_t(note_full);

                    // Note name
                    ssize_t note        = note_number % 12;
                    text.fmt_ascii("lists.notes.names.%s", note_names[note]);
                    lc_string.set(&text);
                    lc_string.format(&text);
                    params.set_string("note", &text);

                    // Octave number
                    ssize_t octave      = (note_number / 12) - 1;
                    params.set_int("octave", octave);

                    // Cents
                    ssize_t note_cents  = (note_full - float(note_number)) * 100 - 50;
                    if (note_cents < 0)
                        text.fmt_ascii(" - %02d", -note_cents);
                    else
                        text.fmt_ascii(" + %02d", note_cents);
                    params.set_string("cents", &text);

                    wNote->text()->set("lists.notes.display.full_single", &params);
                }
                else
                    wNote->text()->set("lists.notes.display.unknown_single", &params);
            }
        }



        status_t filter_ui::post_init()
        {
            status_t res = ui::Module::post_init();
            if (res != STATUS_OK)
                return res;

            wNote         = filter_widget<tk::GraphText>("filter_note");
            pType         = find_port("ft");
            pFreq         = find_port("f");

            if (pType != NULL)
                pType->bind(this);
            if (pFreq != NULL)
                pFreq->bind(this);

            update_filter_note_text();

            return STATUS_OK;
        }


        void filter_ui::notify(ui::IPort *port)
        {
                    update_filter_note_text();
        }

    } /* namespace plugins */
} /* namespace lsp */
