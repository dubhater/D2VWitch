v3 (tbd)
========

* Add support for H264 in transport streams and elementary streams.
  d2vsource v2.0 or newer is needed.

* Fix incorrectly reported codec type ("unknown") with some newer
  ffmpeg versions. This may break compilation with older ffmpeg
  versions.

* Hopefully fix the freezing of the graphical interface in Windows
  when clicking "Add files" or "Browse".

* Avoid leaving behind empty files when the audio decoders can't be
  opened.

* Skip undecodable leading non-keyframes.

* Skip leading B frames in MPEG2 if the first GOP is open.

* Fix incorrect flagging of some MPEG2 closed GOPs as open.

* Fix incorrect flagging of leading B frames in closed GOPs in MPEG2
  streams.

* If the colormatrix is undefined, guess it from the resolution.

* Include the pixel format in the output of ``--info``.



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
