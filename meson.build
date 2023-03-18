project('d2vwitch', 'cpp',
        version: '5',
        default_options: ['cpp_std=c++11', 'buildtype=release'],
        meson_version: '>=0.46')

qt5 = import('qt5')

deps = [
  dependency('vapoursynth').partial_dependency(includes: true, compile_args: true),
  dependency('libavcodec'),
  dependency('libavformat'),
  dependency('libavutil'),
  dependency('qt5', modules: ['Core', 'Gui', 'Widgets'])
]

moc_headers = [
  'src/GUIWindow.h',
  'src/ListWidget.h',
  'src/ScrollArea.h'
]

processed_files = qt5.preprocess(moc_headers: moc_headers)

sources = [
  'src/Audio.cpp',
  'src/Audio.h',
  'src/Bullshit.cpp',
  'src/Bullshit.h',
  'src/D2V.cpp',
  'src/D2V.h',
  'src/D2VWitch.cpp',
  'src/FakeFile.cpp',
  'src/FakeFile.h',
  'src/FFMPEG.cpp',
  'src/FFMPEG.h',
  'src/GUIWindow.cpp',
  'src/GUIWindow.h',
  'src/ListWidget.cpp',
  'src/ListWidget.h',
  'src/MPEGParser.cpp',
  'src/MPEGParser.h',
  'src/ScrollArea.cpp',
  'src/ScrollArea.h',
  processed_files
]


warnings = [
  '-Wall',
  '-Wextra',
  '-Wshadow'
]

cpp_args = [
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  warnings
]

executable('d2vwitch',
  sources: sources,
  dependencies: deps,
  gui_app: true,
  cpp_args: cpp_args,
  install: true)
