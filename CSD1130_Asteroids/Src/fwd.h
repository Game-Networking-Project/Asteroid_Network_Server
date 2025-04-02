#ifndef SIMPLE_UDP_FWD_FRAMEWORK_H
#define SIMPLE_UDP_FWD_FRAMEWORK_H

namespace udpf_impl{
	//info classes
	struct endpoint_addr;									//holds endpoint addr
	struct endpoint_config;									//holds endpoint configurations
	struct listener_config;
	struct connection_config;

	//data frame classes
	struct endpoint_frame;
	struct listener_frame;
	struct connection_frame;

	//connection classes
	struct endpoint;
	struct connection;
	struct listener;
	struct endpoint_list;

	using sockfds = unsigned __int64;

	using u8 = unsigned __int8;
	using u16 = unsigned __int16;
	using u32 = unsigned __int32;
	using u64 = unsigned __int64;

	using i8 = __int8;
	using i16 = __int16;
	using i32 = __int32;
	using i64 = __int64;

	using frame_ptr = char*;								//make sure this is not an ordinary char* but the char* that points to the frame
	using endpoint_descriptor = u64;
}

#endif