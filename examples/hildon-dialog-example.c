/*
 * This file is a part of hildon examples
 *
 * Copyright (C) 2008 Nokia Corporation, all rights reserved.
 *
 * Author: Victor Jaquez <vjaquez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include                                        "hildon.h"

int
main                                            (int argc,
                                                 char **args)
{
    HildonDialog *d;
    GtkWidget *label;

    gtk_init (&argc, &args);

    d = HILDON_DIALOG (hildon_dialog_new ());
    label = gtk_label_new ("Hello, world!");

    gtk_window_set_title (GTK_WINDOW (d), "Hi!");
    gtk_dialog_add_button (GTK_DIALOG (d), GTK_STOCK_OK, GTK_RESPONSE_NONE);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(d)->vbox), label);

    gtk_widget_show_all (GTK_WIDGET (d));

    gtk_dialog_run (GTK_DIALOG (d));

    return 0;
}
