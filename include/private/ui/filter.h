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

#ifndef PRIVATE_UI_FILTER_H_
#define PRIVATE_UI_FILTER_H_

#include <lsp-plug.in/plug-fw/ui.h>
#include <lsp-plug.in/lltl/darray.h>

namespace lsp
{
    namespace plugui
    {
        // UI for Filter plugin series
        class filter_ui: public ui::Module, public ui::IPortListener
        {
            protected:
                ui::IPort          *pType;
                ui::IPort          *pFreq;
                ui::IPort          *pGain;

                tk::GraphText      *wNote;          // Text with note and frequency

            protected:
                template <class T>
                T              *filter_widget(const char *widget_id);

            protected:

                ui::IPort      *find_port(const char *port_id);
                void            update_filter_note_text();

            public:
                explicit filter_ui(const meta::plugin_t *meta);
                virtual ~filter_ui() override;

                virtual status_t    post_init() override;

                virtual void        notify(ui::IPort *port, size_t flags) override;
        };
    } /* namespace plugui */
} /* namespace plugui */

#endif /* PRIVATE_UI_FILTER_H_ */
