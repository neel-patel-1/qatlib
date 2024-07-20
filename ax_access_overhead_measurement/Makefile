deser_and_hash: main.cpp
	pkg-config --cflags protobuf	# fails if protobuf is not installed
	g++ -o deser_and_hash main.cpp src/router.pb.cc `pkg-config --cflags --libs protobuf` \
	-ldml -I./include -I../include

walker_overhead: walker_ax_access_overhead.cpp
	g++ -o walker_overhead walker_ax_access_overhead.cpp src/dsa_alloc.c \
	-ldml -I./include -I../include