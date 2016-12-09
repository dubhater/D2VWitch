v2 (20161209)
=============

* Rename the executable from ``D2VWitch`` to ``d2vwitch`` because no
  one likes typing commands with capital letters in them.

* Remux LPCM audio into Wave 64 instead of producing unusable audio
  files.

* Guess the channel layout for LPCM audio on DVDs based on the number
  of channels, instead of saying it has zero channels.

* Fix occasional crash when demuxing LPCM audio.

* Give audio files proper extensions instead of ``.audio``.

* Add option ``--input-range``, which sets the YUVRGB_Scale field
  according to the video's colour range. Make it default to limited
  range.

* Add a graphical interface, launched when no command line parameters
  are supplied.



v1 (20160116)
=============

* Initial release.
