# Hacking

To view the documentation, run
```sh
make doc
```
and browse it via `./src/html/index.html`.

## Design overview

The lowest level part of the code is the sound backend. Only Alsa is supported
at the moment, but more backends may be added in the future.

The backend is hidden behind a frontend, defined in `audio.c`. Only `audio.c`
deals with audio backends. This means that the whole of the code is blissfully
ignorant of the audio backend in use.

`audio.c` is also in charge of emitting signals whenever a change happens.
This means that PNMixer design is quite *signal-oriented*, so to say.

The ui code is nothing fancy. Every widget strives to be standalone.
It access the sound system with function calls.
It listens to signals to update its appearance.

There's something you should keep in mind. Audio on a computer is a shared
resource. PNMixer isn't the only one that can change it.
At any moment the audio volume may be modified by someone else,
and we must update the ui accordingly. So listening to changes from
the audio subsystem (and therefore having a *signal-oriented* design)
is the most obvious solution to solve that problem.

## Coding style

This is more or less kernel coding style. Try to match the surroundings.
For automatic code indentation we use [astyle](http://astyle.sourceforge.net/).
Please run `make indent-code` if you want to indent the whole sources.

To indent the xml ui files, we use [xmllint](http://xmlsoft.org/xmllint.html).
Please run `make indent-xml` if you want to indent the xml ui files.

## Comments

* comments are in doxygen format
* all functions, all data types, all macros, all typedefs... basically everything
* comments where people read them, so preferably at the definition of a function

## Good practices

* use const modifiers whenever possible, especially on function parameters
* if unsure whether to make a function static or not, make it static first
* use unsigned ints instead of signed ints whenever possible

## Translating

In order to update the po files, run the following command:

	cd po && make update-po 

## How to contribute

* [pull request on github](https://github.com/nicklan/pnmixer/pulls)
* email with pull request
* email with patch

## Tips and Tricks

* if you use vim with [youcompleteme](http://valloric.github.io/YouCompleteMe/), you can use the following [.ycm_extra_conf.py](https://gist.github.com/hasufell/0a97cc13de3ef2f061bb)
