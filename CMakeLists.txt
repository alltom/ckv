cmake_minimum_required (VERSION 2.6)
project (ckv)
add_subdirectory (audio)

add_executable (ckv ckv ckvm luabaselite pq rtaudio_wrapper)

target_link_libraries (ckv audio ugen asound lua rtaudio pthread avformat avcodec avutil swscale z)


