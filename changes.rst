v3 (20190404)
=============

* Fix incorrectly reported codec type ("unknown") with some newer
  ffmpeg versions. This may break compilation with older ffmpeg
  versions.

* Hopefully fix the freezing of the graphical interface in Windows
  when clicking "Add files" or "Browse", by using native file
  selectors.

* Avoid leaving behind empty files when the audio decoders can't be
  opened.

* Skip undecodable leading (in coded order) non-keyframes. It helps
  produce more correct d2v files with imperfect streams.

* Skip unknown picture types. They probably can't be decoded.

* Fix incorrect flagging of some MPEG2 closed GOPs as open.

* Fix incorrect flagging of leading B frames in closed GOPs in MPEG2
  streams.

* Avoid "Could not connect to display" error when running without X.

* Don't ignore the ``--video-id`` parameter.

* Avoid the long delay when adding or removing files experienced by
  some Windows users with NTFS filesystems. The downside is that the
  D2V file name box won't necessarily turn red when the selected
  folder is not writable.

* Don't make the D2V file name box red when there are no video files.

* Fix an unlikely memory leak in the LPCM demuxing code.

* Fix a small memory leak that was always present.

* If the colormatrix is undefined, guess it from the resolution.

* Use backslashes instead of forward slashes in file names in Windows.

* Include the pixel format in the output of ``--info``.

* Write ``input.d2v`` instead of ``input.vob.d2v``,
  ``input T80 blah.ac3`` instead of ``input.vob.d2v T80 blah.ac3``,
  ``input.1000-2000.m2v`` instead of ``input.d2v.1000-2000.m2v``,
  ``input.1000-2000.d2v`` instead of ``input.d2v.1000-2000.m2v.d2v``.

* Make the graphical interface warn before overwriting any files.

* Make the command line interface automatically index files in
  ``vts_xx_y.vob`` sequences. The graphical interface doesn't do this
  because it's much easier to select all the relevant files there.

* Add option ``--single-input`` to disable the automatic indexing of
  ``vts_xx_y.vob`` sequences.

* Calculate the delay required by each demuxed audio track and write
  it into the audio file name. As part of this feature, the audio
  tracks are no longer demuxed from the beginning of the file, but
  from the first video keyframe onwards. This seems to be what DGIndex
  does.

* Add option ``--ffmpeg-log-level``.

* Add option ``--relative-paths`` and a corresponding check box in the
  graphical interface, to write relative paths in d2v files.

* Write ``d2vwitch.ini`` next to ``d2vwitch.exe`` in order to remember
  some settings, like the one about relative paths. (Technically it's
  ``<executable name>.ini``.)

* Make the graphical interface remember the size and position of the
  main window.



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
