FROM wiktorguz/qt6

RUN apt update && apt upgrade -y
RUN apt install -y software-properties-common
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test && apt update
RUN apt install -y libswscale-dev libswresample-dev libpostproc-dev libavutil-dev libavresample-dev libavformat-dev libavfilter-extra libavfilter-dev libavdevice-dev libavcodec-extra libavcodec-dev ffmpeg git cmake libvulkan-dev gcc-10 gcc-10-base gcc-10-doc g++-10 libopenal-dev

COPY build.sh /home/qt/

CMD ["/home/qt/build.sh"]
