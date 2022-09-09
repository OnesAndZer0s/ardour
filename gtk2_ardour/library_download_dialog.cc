/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>

#include "pbd/i18n.h"

#include <glibmm/markup.h>

#include "ardour/rc_configuration.h"
#include "ardour/library.h"

#include "library_download_dialog.h"

using namespace ARDOUR;
using std::string;

LibraryDownloadDialog::LibraryDownloadDialog ()
	: ArdourDialog (_("Loop Library Manager"), true) /* modal */
{
	_model = Gtk::ListStore::create (_columns);
	_display.set_model (_model);

	_display.append_column (_("Name"), _columns.name);
	_display.append_column (_("Author"), _columns.author);
	_display.append_column (_("License"), _columns.license);
	_display.append_column (_("Size"), _columns.size);
	_display.append_column_editable (_("Installed"), _columns.installed);

	Gtk::CellRendererToggle *toggle = dynamic_cast<Gtk::CellRendererToggle *> (_display.get_column_cell_renderer (4));
	toggle->set_alignment (0.0, 0.5);
	toggle->signal_toggled().connect (sigc::mem_fun (*this, &LibraryDownloadDialog::install_activated));

	_display.set_headers_visible (true);
	_display.set_tooltip_column (5); /* path */

	Gtk::HBox* h = new Gtk::HBox;
	h->set_spacing (8);
	h->set_border_width (8);
	h->pack_start (_display);

	get_vbox()->set_spacing (8);
	get_vbox()->pack_start (*Gtk::manage (h));


	ARDOUR::LibraryFetcher lf;
	lf.get_descriptions ();
	lf.foreach_description (boost::bind (&LibraryDownloadDialog::add_library, this, _1));
}

void
LibraryDownloadDialog::add_library (ARDOUR::LibraryDescription const & ld)
{

	Gtk::TreeModel::iterator i = _model->append();
	(*i)[_columns.name] = ld.name();
	(*i)[_columns.author] = ld.author();
	(*i)[_columns.license] = ld.license();
	(*i)[_columns.size] = ld.size();
	(*i)[_columns.installed] = ld.installed();
	(*i)[_columns.url] = ld.url();

	/* tooltip must be escape for pango markup, and we should strip all
	 * duplicate spaces
	 */

	(*i)[_columns.description] = Glib::Markup::escape_text (ld.description());
	std::cerr << "set descr to " << ld.description() << std::endl;
}


void
LibraryDownloadDialog::install_activated (std::string str)
{
	Gtk::TreeModel::iterator row = _model->get_iter (Gtk::TreePath (str));
	std::string url = (*row)[_columns.url];

	std::cerr << "will download " << url << " to " << Config->get_clip_library_dir() << std::endl;

	ARDOUR::Downloader* downloader = new ARDOUR::Downloader (url, ARDOUR::Config->get_clip_library_dir());

	/* setup timer callback to update progressbar */

	Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (*this, &LibraryDownloadDialog::dl_timer_callback), downloader, str), 40);

	/* and go ... */

	downloader->start ();

	(*row)[_columns.downloader] = downloader;

	/* and back to the GUI, though we're modal so not much is possible */
}

bool
LibraryDownloadDialog::dl_timer_callback (Downloader* dl, std::string path_str)
{
	double prog = dl->progress();

	std::cerr << "prog: " << prog << std::endl;

	/* set something based on this */
	if (dl->status() != 0) {
		delete dl;
		Gtk::TreeModel::iterator row = _model->get_iter (Gtk::TreePath (path_str));
		(*row)[_columns.downloader] = 0;
		return false; /* no more calls, done or cancelled */
	}
	return true;
}