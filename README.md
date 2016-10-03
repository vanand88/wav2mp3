# wav2mp3
Convert wave files to mp3 format using lame encoder library (multithreaded).

# Usage
wav2mp3 \<wav_files_path\>

# Specifications
Supported platforms: Windows, Linux.
Uses lame encoder library to convert wav file into mp3 file.
Uses POSIX threads for concurrent conversion. Creates as many threads as many CPU cores are available.
Non-wav files are skipped (file extensions are used to identify file type).
Output mp3 files are placed in the same directory.

# Source files
main.cpp

# Build under linux
Use make command for build. make options - prep (creates output directories), debug, release, clean, all.
Creates output file in the corresponding directory (debug or release).

#Build under windows
Visual Studio project and solution files are included.
Creates output file in the corresponding directory (Debug or Release)

Requires pthreadVC2.dll to be placed in the same directory as executable (.exe) or in windows system path.
2 copies of pthreadVC2.dll file are placed in the output directories (Debug and Release).

A default command-line option is added to Visual Studio project settings: wav file path ./wav/ is used as an argument when running from inside Visual Studio.

