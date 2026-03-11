# figpad - GTK4 based simple text editor


Description
===========

figpad is a simple GTK4+ text editor that emphasizes simplicity. As development
focuses on keeping weight down to a minimum, only the most essential features
are implemented in the editor. figpad is simple to use, is easily compiled,
requires few libraries, and starts up quickly.

figpad is based on [l3afpad](https://github.com/stevenhoneyman/l3afpad) which itself
is a GTK3 port of GTK2 [leafpad](http://tarot.freeshell.org/leafpad/).

This program is released under the GNU General Public License (GPL) version 2,
see the file 'COPYING' for more information.


Features
========

* Codeset option (Some OpenI18N registered)
* Auto codeset detection (UTF-8 and some codesets)
* Unlimitted Undo/Redo
* Auto/Multi-line Indent
* Display line numbers
* Drag and Drop
* Printing


Installation
============

figpad requires:

* GTK+-4.x.x libraries
* ncurses library

and for building also:

* automake
* intltool

Simple install procedure:

```sh
$ tar xzvf figpad-x.x.x.tar.gz       # unpack the sources
$ cd figpad-x.x.x                    # change to the toplevel directory
$ ./autogen.sh                       # generate the `configure' script
$ ./configure                        # run the `configure' script
$ make                               # build
# [ Become root if necessary ]
# make install-strip                 # install
```

Keybindings
===========

| Keybinding                     | Action             |
| ------------------------------ | ------------------ |
| Ctrl-N                         | New                |
| Ctrl-O                         | Open               |
| Ctrl-S                         | Save               |
| Shift-Ctrl-S                   | Save As            |
| Ctrl-W                         | Close              |
| Ctrl-P                         | Print              |
| Ctrl-Q                         | Quit               |
| Ctrl-Z                         | Undo               |
| Shift-Ctrl-Z (Ctrl-Y)          | Redo               |
| Ctrl-X                         | Cut                |
| Ctrl-C                         | Copy               |
| Ctrl-V                         | Paste              |
| Ctrl-A                         | Select All         |
| Ctrl-F                         | Find               |
| Ctrl-G (F3)                    | Find Next          |
| Shift-Ctrl-G (Shift-F3)        | Find Previous      |
| Ctrl-H (Ctrl-R)                | Replace            |
| Ctrl-J                         | Jump To            |
| Ctrl-T                         | Always on Top      |
| Ctrl-Tab                       | Toggle tab width   |
| Tab with selection bound       | Multi-line indent  |
| Shift-Tab with selection bound | Multi-line unindent|
