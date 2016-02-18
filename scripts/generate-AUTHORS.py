#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2016 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

from __future__ import (unicode_literals, print_function, division)

import sys
import subprocess
import codecs
import locale

blacklist = (
	# Unknown
	"(no author) <(no author)@05730e5d-ab1b-0410-a4ac-84af385074fa>",
	"root <root@DiskStation.(none)>",
	"unknown <LoL@.(none)>",
	"unknown <d0t@.(none)>",

	# Bots
	"MumbleTransifexBot <mumbletransifexbot@mumble.info>",

	# Aliases of Bartek "stranded" Sumowski <sumowski@gmail.com>
	"Bartek <sumowski@users.sourceforge.net>",
	"Bartek Sumowksi <sumowski@gmail.com>",

	# Alias of Benjamin Jemlich <pcgod@users.sourceforge.net>
	"Benjamin Jemlich <pcgod@gmx.net>",

	# Alias of EarlOfWenc <lorenz.schwittmann@gmail.com>
	"EarlOfWenc <user@localhost>",

	# Aliases of Jamie Fraser <fwagglechop@gmail.com> -- latest commit uses this email.a
	"Jamie Fraser <jamie.f@mumbledog.com>",
	"Jamie Fraser <jamie.f@sabrienix.com>",

	# Aliases of Álvaro Manuel Recio Pérez <naproxeno@users.sourceforge.net>
	"Álvaro M. Recio Pérez <naproxeno@users.sourceforge.net>",
	"Álvaro Manuel Recio Pérez <naproxeno@kepis.(none)>",

	# Aliases of Thorvald Natvig <slicer@users.sourceforge.net>
	"Thorvald Natvig <github@natvig.com>",
	"Thorvald Natvig <thorvald@-e false.(none)>",
	"Thorvald Natvig <thorvald@debian.localdomain>",
	"Thorvald Natvig <thorvald@natvig.com>",

	# Alias of Spaccaossi <spaccaossi@gmail.com>
	"Spaccaossi <spaccaossi@users.sourceforge.net>",

	# Aliases of Stefan Hacker <dd0t@users.sourceforge.net>
	"Stefan H <dd0t@users.sourceforge.net>",
	"Stefan Hacker <hacker.stefan@googlemail.com>",
	"Stefan Hacker <hast@hast-desktop.(none)>",

	# Aliases of Michał "Zuko" Żukowski <zuczeq@gmail.com>
	"Michał Żukowski <zuczeq@gmail.com>",
	"Zukowski Michal <zuczeq@gmail.com>",
	"Żukowski Michał <zuczeq@gmail.com>",
	"zuczeq <zuczeq@users.sourceforge.net>",
	"Zuko <zuczeq@gmail.com>",

	# Two authors?! Both are listed elsewhere, so drop.
	"Michael Ziegler and Natenom <natenom@googlemail.com>",

	# ...What?
	"Netbios Domain Administrator <admin@gameserver2.(none)>",

	# Aliases of Mikkel Krautz <mikkel@krautz.dk>
	"Mikkel <mikkel@krautz.dk>",
	"Mikkel Krautz <mkrautz@users.sourceforge.net>",

	# Alias of tkmorris <mauricioarozi@gmail.com>
	"morris <tkmorris@users.sourceforge.net>",

	# Alias of bendem <online@bendem.be>
	"bendem <bendembd@gmail.com>",

	# Alias of arrai <array.of.intellect@gmail.com>
	"Arrai <arrai@users.sourceforge.net>",

	# Alias of Joël Troch <joel.troch62@gmail.com>
	"Joël Troch <https://github.com/JoelTroch>",

	# Matthias Vogelgesang <matthias.vogelgesang@gmail.com>
	"Matthias Vogelgesang <m0ta@users.sourceforge.net>",

	# Alias of Tuck Therebelos <snares@users.sourceforge.net>
	"Snares <snares@users.sourceforge.net>",

	# Aliases of Natenom <natenom@natenom.com>
	"Natenom <natenom@googlemail.com>",
	"Natenom <natenom@natenom.name>",

	# Alias of haru_arc <arcenciel@users.sf.net>
	"Arcenciel <arcenciel@users.sourceforge.net>",
	"arcenciel <arcenciel@users.sourceforge.net>",

	# Alias of Jonathan E. Hansen <zentriple@users.sourceforge.net>
	"Jonathan <zentriple@users.sourceforge.net>",
	"zentriple <zentriple@users.sourceforge.net>",
	"Zentriple <zentriple@users.sourceforge.net>",

	# Alias of Patrick Matthäi <pmatthaei@debian.org>
	"Patrick Matthäi <the-me88@users.sourceforge.net>",

	# Alias of Jan Klass <kissaki@gmx.de>
	"Jan Klass <kissaki0@gmail.com>",

	# Alias of Semion Tsinman <Necromancer3333@gmail.com>
	"Necromancer <necromancer3@users.sourceforge.net>",
	"Necromancer <necro3@users.sourceforge.net>",

	# Alias of Svenne33 <svenne33@users.sourceforge.net>
	"svenne33 <svenne33@users.sourceforge.net>",

	# Potentially an alias of BAYSSE Laurent <lolo_32@users.sourceforge.net>
	"lolo_32 <Alex@.(none)>",

	# Alias of Kevin Rohland <kevin@nascher.org>
	"SuperNascher <kevin@nascher.org>",
)

# These are authors whose patches were applied during
# the time when Mumble used SVN for version control.
# The patches themselves are in Git, but the author
# and commiter metadata in Git does not reflect the
# actual patch author.
# This list has been manually compiled by looking
# through the Git history.
patchAuthors = (
	# Via 6cc47c35cac16877e119cef35d9255918849495d
	"Opalium <opalium@users.sourceforge.net>",

	# Via d16876d804d028153f37f4f8aff770469edf6997
	"Marc Deslauriers <marc.deslauriers@canonical.com>",

	# Via bd690db64560cd1785c9aaed86547ffe681b60db
	"Otto Allmendinger <oallmendinger@users.sourceforge.net>",

	# Via 143589b1a8f264026c12f974e084030eaba51428
	"ozon <mumble-tower@users.sourceforge.net>",

	# Via 1b048e6c1516eee4f932f9f9eeeb6b13de5d15cb
	"Cesare Tirabassi <cesare.tirabassi@gmail.com>",

	# Via ddfc033c3c20a67994ef6329691f2a3616a346c0
	"Stefan Gehn <mETz@gehn.net>",

	# Via c317abec96d9007cc7ca95f54e5193b48268e7ad
	"Thibault Capdevielled <theblackstorm@userse.sourceforge.net>",

	# Via 51510ff8f744adc281c4eca6c0fdef5ba2ab9cbb
	"Jan Braun <janbraun@gmx.net>",

	# Via 887368a9fb5a6d35c3750a5f48d1293d4fc58df3
	"Balazs Nagy <julian7@users.sourceforge.net>",

	# Via 7814dba5f86b69f446e45c639ae3db7adfaf9c48
	"Jerome Vidal <jerhum@users.sourceforge.net>",

	# Via 82e4966bba8d390e22f43d15764c6c272dd91e2b
	"Matt M. <mokomull@users.sourceforge.net>",

	# Via b40cf89632a8ac46ac92d8a7fa4091c7d035122a
	"mystic_sam <mystic_sam@users.sourceforge.net>",

	# Via 2cecce4270555e4b7fc4c9e3f39c064f72247667
	"Entitaet <entitaet@users.sourceforge.net>",

	# Via 2a6ae358c37ff0c3570ebcb8466aacd673352b95
	"Jakob Dettner <jakobdettner@users.sourceforge.net>",

	# Via b8ff5eed6ef0835b4db2ed160f1e41a045d16db4
	"derandi <derandi@users.sourceforge.net>",

	# Via b2f70a7802e90eea0d04eef7edc0a52178fda753
	"Prosper_Spurius <prosper_spurius@users.sourceforge.net>",

	# Via 9952748561122969d02f2d9ebc49b001ffe82823
	"Leszek Godlewski <inequation@users.sourceforge.net>",

	# Via 2c0a0ff8244966be4d058742a3c01859bad44b37
	"Mark Schreiber <mark7@users.sourceforge.net>",

	# Via 39005601bb92b8f994e8e50887b3bb6535a2c305
	"Sebastian Schlingmann <mit_service@users.sourceforge.net>",

	# Via d2b735799ca37d957def45816766ef266c2b7057
	"Arseniy Lartsev <ars3niy@users.sourceforge.net>",

	# Via 12191680451f0f9d31526908a9c0ded669f05cb8
	"javitonino <javitonino@users.sourceforge.net>",

	# Via 82ffa8948f06f87f58ca7072960d7c54729e7e8b
	"Thibaut Girka",

	# Via a1ba43376db2ec76f2cf244793bfc1565ac88454
	"Jérôme \"buggerone\" <buggerone@users.sourceforge.net>",

	# Via 0d9f5d03426735c4d42e9cd9fe81868b1437aa06
	"Friedrich Uz-Valentin <uz_@users.sourceforge.net>",
)

def gitAuthorsOutput():
	p = subprocess.Popen(["git", "log", "--format=%aN <%aE>", "master", "1.2.x"], stdout=subprocess.PIPE)
	stdout, stderr = p.communicate()
	if stdout is not None:
		stdout = stdout.decode('utf-8')
	if stderr is not None:
		stderr = stderr.decode('utf-8')
	if p.returncode != 0:
		raise Exception('"git log" failed: %s', stderr)
	return stdout

def main():
	locale.setlocale(locale.LC_ALL, "")

	authorsSet = set()
	authorsText = gitAuthorsOutput()
	for line in authorsText.split("\n"):
		if line == '':
			continue

		if line == "zapman <unknown>":
			line = "zapman"

		if line == "Derrick Dymock <derrick@puppetlabs.com>":
			line = "Derrick Dymock <actown@gmail.com>"

		# Fix "=?UTF-8 Michał Żukowski?=" and possibly others like it.
		if line.startswith("=?UTF-8 "):
			line = line.replace("=?UTF-8 ", "")
			line = line.replace("?=", "")

		# Use GitHub URL instead of $user@users.noreply.github.com
		if '@users.noreply.github.com' in line:
			line = line.replace('@users.noreply.github.com', '')
			line = line.replace('<', '<https://github.com/')

		if line in blacklist:
			continue

		authorsSet.add(line)

	for author in patchAuthors:
		authorsSet.add(author)

	f = codecs.open("AUTHORS", "w", "utf-8")
	f.write('''// This is the official list of people who have contributed
// to, and/or hold the copyright to Mumble.
//
// The use of Mumble source code is governed by a BSD-style
// license that can be found in the LICENSE file at the root
// of the Mumble source tree or at <http://www.mumble.info/LICENSE>.
//
// Contributions made on behalf of another entity, such as a
// company are indicated with the following suffix:
//
//     John Doe <jd@mumble.info> (on behalf of $COMPANY)
//
// It is possible to mix individual contributions with company
// contributions. For example, if a contributor, over time,
// has contributed code copyrighted by the contributor, as well
// as various companies:
//
//     John Doe <jd@mumble.info> (individually, on behalf of
//                                $COMPANY1, on behalf of
//                                $COMPANY2, [...]).
//
// Mumble's code is developed in a Git repository. A log of
// every contribution ever made to Mumble is available in the
// Git repository. The Git repository can be queried to get
// detailed authorship information for copyright and attribution
// purposes for each file that makes up the software. A detailed
// analysis of contributions made to Mumble is available via GitHub's
// contribution statistics:
//
// <https://github.com/mumble-voip/mumble/graphs/contributors>

''')

	# Sort alphabetically
	authors = list(authorsSet)
	if sys.version[0] == '2': # Python 2
		authors.sort(cmp=locale.strcoll)
	else:
		authors.sort(key=locale.strxfrm)

	for author in authors:
		f.write(author)
		f.write("\n")

	f.write("""
// Special thanks to:
//
//    Thorvald Natvig, for founding the Mumble project
//    and maintaining it during its formative years.
""")

	f.close()

if __name__ == '__main__':
	main()
