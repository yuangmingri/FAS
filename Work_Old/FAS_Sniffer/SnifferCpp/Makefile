CXX = g++
CXXFLAGS = -I../FAS -I../install/include --std=c++14 -Wall -W -Os
LDLIBS = -L../FAS -lVadDetector -L../install/lib -lavformat -lavcodec -lm -lz -ldl -lavutil -lavfilter -lswresample -lswscale -lpcap -lpcre -lpthread -lboost_system -lrt -ldl


all: rtp_sniffer


rtp_sniffer: main.o call.o rtp_media_stream.o rtp_media_stream_processing.o sip_header.o sip_header_processing.o network_packet.o network_packet_pool.o process_assembler.o process_capture.o process_flusher.o process_worker.o rtp_metadata.o sip_metadata.o media_frame.o
	$(CXX) $(CXXFLAGS) *.o -o rtp_sniffer $(LDLIBS)
	#rm -Rf *.o


main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp


rtp_media_stream.o: rtp_media_stream.cpp
	$(CXX) $(CXXFLAGS) -c rtp_media_stream.cpp


rtp_media_stream_processing.o: rtp_media_stream_processing.cpp
	$(CXX) $(CXXFLAGS) -c rtp_media_stream_processing.cpp


sip_header.o: sip_header.cpp
	$(CXX) $(CXXFLAGS) -c sip_header.cpp


sip_header_processing.o: sip_header_processing.cpp
	$(CXX) $(CXXFLAGS) -c sip_header_processing.cpp


call.o: call.cpp
	$(CXX) $(CXXFLAGS) -c call.cpp


network_packet.o: network_packet.cpp
	$(CXX) $(CXXFLAGS) -c network_packet.cpp


network_packet_pool.o: network_packet_pool.cpp
	$(CXX) $(CXXFLAGS) -c network_packet_pool.cpp


process_assembler.o: process_assembler.cpp
	$(CXX) $(CXXFLAGS) -c process_assembler.cpp


process_capture.o: process_capture.cpp
	$(CXX) $(CXXFLAGS) -c process_capture.cpp


process_flusher.o: process_flusher.cpp
	$(CXX) $(CXXFLAGS) -c process_flusher.cpp


process_worker.o: process_worker.cpp
	$(CXX) $(CXXFLAGS) -c process_worker.cpp


rtp_metadata.o: rtp_metadata.cpp
	$(CXX) $(CXXFLAGS) -c rtp_metadata.cpp


sip_metadata.o: sip_metadata.cpp
	$(CXX) $(CXXFLAGS) -c sip_metadata.cpp


media_frame.o: media_frame.cpp
	$(CXX) $(CXXFLAGS) -c media_frame.cpp

clean:
	rm -Rf rtp_sniffer *.o
