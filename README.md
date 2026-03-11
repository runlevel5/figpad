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

* GTK4 (>= 4.16.0)

and for building also:

* meson (>= 0.59)
* ninja

Simple install procedure:

```sh
$ meson setup builddir
$ meson compile -C builddir
$ meson install -C builddir          # may require root
```

Build options can be set with `meson configure`:

```sh
$ meson configure builddir -Denable_print=true
$ meson configure builddir -Denable_statistics=true
$ meson configure builddir -Denable_search_history=false
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
