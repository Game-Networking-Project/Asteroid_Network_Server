#ifndef SIMPLE_UDP_FRAMEWORK_H
#define SIMPLE_UDP_FRAMEWORK_H

#include "fwd.h"
#include <map>
#include <queue>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_map>

#ifdef SIMPLE_UDP_FRAMEWORK_IMPL
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif

#define PING_COUNT 3
#define STREAM_PROCESSING_THREAD_COUNT 3
#define MSG_BUFFER_MAX 3

namespace udpf_impl {
	//info classes
	struct endpoint_addr {										//holds endpoint addr
		union {
			char un[4];
			u32	netip;
		}ipv4;
		u16 netport;
	};

	struct endpoint_config {									//holds endpoint configurations
		u32 max_connection;
	};

	struct listener_config {
		std::vector<u32> block_netipv4;
		u32 request_timeout_mili;
	};

	struct connection_config {
		u16 max_datagram_size;
		u16 max_window_size;
		u32 connection_timeout_mili;
		u32 packet_timeout_mili;
	};

	//data frame classes
	struct endpoint_frame {	//frame structure [crc8|type|crc_div|padding/unused]
		u8 crc8_cs;
		u8 type;
		u8 crc_pdiv;
		u8 _padding; //unused

		enum frame_types {
			NULL_FRAME,
			LISTENER_FRAME,
			CONNECTION_FRAME,
			STREAM_FRAME,
		};

		endpoint_frame();
		endpoint_frame(frame_ptr, u32 frame_size);

		bool is_corrupt();
		void flush_to_buffer(char* buff, u64 size);

		static constexpr u64 frame_size{ sizeof(u8) * 4 };
	};

	struct listener_frame {  //message format {endpoint frame}|[crc|type|crc_pdiv|in_addr_ip|in_addr_port|in_config -> follow member declaration order]
		u8 crc8_cs;
		u8 type;
		u8 crc_pdiv;
		endpoint_addr in_addr;
		connection_config in_config;

		static constexpr u64 frame_size{ sizeof(u8) * 3 + sizeof(u16) * 3 + sizeof(u32) * 3 };
		static constexpr u64 transport_frame_size{ endpoint_frame::frame_size + frame_size };

		listener_frame();
		listener_frame(frame_ptr, u32 frame_size);
		bool is_corrupt();
		void flush_to_buffer(char* buff, u32 buff_size);
		std::unique_ptr<char> make_transport_frame();
	};

	struct connection_frame { //message format {endpoint frame}|[crc|type|seq_num|crc_pdiv|payload_len]
		u16 crc16_cs;
		u8 type;
		u32 seq_num;
		u16 crc_pdiv;
		u16 payload_len;
		char* payload;

		enum ctypes {
			INIT_HANDSHAKE = 0x0,
			DATA = 0x1,
			ACK = 0x1 << 1,
			BUFFER_FULL = 0x1 << 2,
			FIN = 0x1 << 3,
			CONN_ERROR = 0x1 << 4,
			SYN = 0x1 << 5,
			PING = 0x1 << 6
		};

		static constexpr u64 frame_header_size{ sizeof(u8) + sizeof(u16) * 3 + sizeof(u32) };
		static constexpr u64 transport_frame_header_size{ endpoint_frame::frame_size + frame_header_size };

		static constexpr u64 transport_init_handshake_frame_size{ transport_frame_header_size + listener_frame::transport_frame_size };

		connection_frame();
		connection_frame(frame_ptr, u32 frame_size);
		bool is_corrupt();

		bool is_init_handshake();
		bool is_ping();
		bool is_ack();
		bool is_fin();
		bool is_bfull();
		bool is_cerror();
		bool is_data();

		std::unique_ptr<char> make_transport_frame(); //transport frame structure[[endpoint frame] | [connection_frame]];
	};

	struct stream_frame { //message format {endpoint frame}|[crc|streamid|type|seq_num|crc_pdiv|payload_len]
		u16 crc16_cs;
		u32 stream_id;
		u8 type;
		u32 seq_num;
		u16 crc_pdiv;
		u16 payload_len;
		char* payload;

		enum stypes {
			DATA = 0x0,
			ACK = 0x1,
			SYN = 0x1 << 1,
			FIN = 0x1 << 2
		};

		static constexpr u64 frame_header_size{ sizeof(u8) + sizeof(u16) * 3 + sizeof(u32) * 2 };
		static constexpr u64 transport_frame_header_size{ endpoint_frame::frame_size + frame_header_size };

		struct syn_frame {	//to be carried over stream frame and endpoint frame
			u32 total_message_size;
			syn_frame();
			syn_frame(frame_ptr, u32 frame_size);

			static constexpr u64 transport_frame_size{ transport_frame_header_size + sizeof(u32) };
		};

		stream_frame();
		stream_frame(frame_ptr, u32 frame_size);
		bool is_corrupt();

		bool is_data();
		bool is_ack();
		bool is_syn();
		bool is_fin();

		std::unique_ptr<char> make_transport_frame(); //transport frame structure[[endpoint frame] | [stream_frame]];
		std::unique_ptr<char> make_syn_transport_frame(u32); //transport frame structure[[endpoint frame] | [stream_frame]];
	};

	//connection classes
	struct stream {
		using state = u32;
		using sequence_number = u32;
		connection* conn;
		frame_ptr buffer{};
		u64 buffer_item_count{};
		u64 s_base{};
		u64 s_seek{};									//only used in send mode
		std::mutex s_mtx;
		std::mutex s_queue_mtx;
		std::mutex s_ack_mtx;
		std::mutex s_buffer_mtx;
		std::mutex s_incoming_mtx;
		std::mutex s_outgoing_mtx;
		std::mutex s_producer_mtx;
		std::mutex s_consumer_mtx;
		std::condition_variable cv_incoming;
		std::condition_variable cv_outgoing;
		std::condition_variable cv_producer;
		std::condition_variable cv_consumer;
		std::vector<bool> s_ack;
		std::queue<std::pair<sequence_number, std::optional<std::chrono::steady_clock::time_point>>> seq_sent_queue;	//in recv mode: ack all in seq_sent queue. in send mode: check seq_sent_queue for timeout
		u32	s_ack_ipos{};

		//std::thread consumer_thread;					//process buffer and rdt receive
		std::queue<std::unique_ptr<char>> buffered_messages;
		u64 buffered_msg_count{};
		char* s_store{};								//in recv mode: a dynamically allocated store. in send mode: a pointer to the supplied buffer store (IMPORTANT!: this does not take ownership of the buffer store. please ensure that buffer store is valid during the duration of send)
		u64 s_store_size{};
		state s_state;

		enum states {
			READY,
			RECV,										//stream is in recv mode
			SEND,										//stream is in send mode
			BUFF_FULL,									//stream store buffer is full
			CLOSE,
			CONN_ERROR
		};

		static constexpr u64 READ_STREAM{ 1 };
		static constexpr u64 WRITE_STREAM{ 0 };

		operator bool();

		void process_incoming();
		void process_outgoing();
		void process_send_ack(int ack_type, int target_stream, int seq_num);
		void process_flush_store_to_buffered();

		void process_incoming_stream_frame_to_store(stream_frame&);

		void process_stream();

		stream();
		stream(stream&&) noexcept;
		~stream();

		void ack(sequence_number);
		bool is_acked(sequence_number);

		void reset(connection_config);

		//void send_ack(sequence_number);
		void move_sliding_window_to_last_unack();
		static void stream_processing_thread_wrapper(stream&);
	};

	struct connection {
		using state = u32;
		endpoint_addr local;
		endpoint_addr remote;
		connection_config config;						//negotiated connection params. we assume this to be set only once during initial handshake: determines datagram size and window
		frame_ptr buffer{};								//note: contains endpoint frame also
		u64 buffer_item_count;

#ifdef COMMENTS2
		//char* store;									//stream internal buffer store for datagram reassembly    ->    circular buffer store
#else
		char* store{};									//reduce complexity, udp-like message oriented store buffer. no more tcp-like streams, store only stores 1 completed message. BUFFER_FULL is return even if store contains sufficient buffer for the next message		
#endif
		std::queue<std::unique_ptr<char>> buffered_messages;		//might break if message is in the gb range or computer shit
#ifdef COMMENTS2
		//char* store;									//stream internal buffer store for datagram reassembly    ->    circular buffer store
		u64 read_pos;									//store read window for upper layer store buffer extraction
		u64 write_pos;									//store write 
#endif
		u64 store_size;

		std::vector<stream> streams;					//index 0 write stream, 1 read stream, hardcode 2 streams (1 write, 1 read). DO NOT PUSH BACK THIS (not threadsafe) //this is not meant to be scalable. vector used cos i lazy include array

		std::vector<bool> s_ack;
		u64 s_base;
		//u16 s_wnd;
		std::mutex cn_mtx;
		std::chrono::steady_clock::time_point last_resp_timestamp;	//peer's last response includes ping also
		std::mutex cn_read_mtx;
		std::mutex cn_write_mtx;
		std::condition_variable cv_write;
		std::condition_variable cv_read;
		std::condition_variable cv_read_buffer;
		std::thread read_thread;
		state cn_state;

		/********************************************************************************************************
		message based implementation buffers upto buffered message in the forward list in first in first out order, BUFFER_FULL is return if completed store is not picked
		up by upper layer

		[buffer read input]	-> processing store				 <-   packet assembly. buffer is resized to fit message
									\
								completed queue of stores    <-   upper layer recv() calls retrieves this data	(cv read & cv_write)
		********************************************************************************************************/

		/********************************************************************************************************
		stream based implementation buffers upto buffer size, BUFFER_FULL is return if buffer is full and not picked
		up by upper layer

		[buffer read input]	-> processing store    <-   packet assembly. buffer is fixed size (circular buffer)
								|
								|=> cv read is used to notify consumer that buffer is ready for extraction
		********************************************************************************************************/


		enum states {
			WAIT_FOR_INIT,
			READY,
			RECV,
			SEND,
			BUFF_FULL,
			CLOSE,
			CONN_ERROR
		};

		operator bool();

		connection();
		connection(connection&&) noexcept;
		~connection();

		connection& operator=(connection&&) noexcept;

		void read_buffer();
		static void read_buffer_thread_wrapper(connection&);
		void send_init_req(connection_config&);
		void send_ping();
		void init(connection_config&);
		void close();

		void send(char* data, u64 size);
		std::unique_ptr<char> recv();
	};
	struct listener {
		using state = u32;
		using endpoint_connection_request = std::pair<endpoint_addr, connection_config>;
		std::condition_variable cv;
		std::mutex listener_mtx;
		endpoint_addr local;
		listener_config config;
		u64 item_count{};
		state lr_state;
		char lf_buffer[listener_frame::transport_frame_size];

		enum states {
			CLOSED,
			LISTENING,
			LIS_ERROR
		};

		void close();
		std::optional<endpoint_connection_request> listen();
		connection& accept(endpoint_connection_request&);
	};
	struct endpoint {
		endpoint_addr local;
		endpoint_config	config;
		sockfds	sfd;
		char ef_buffer[endpoint_frame::frame_size];
		std::map<endpoint_descriptor, connection> cn;
		listener lr;
		std::mutex ep_mtx;
		std::mutex send_mtx;
		std::mutex recv_mtx;

		bool recv();
		void discard();
		void send(frame_ptr frame, endpoint_addr& dst, u64 size);
		void close();

		endpoint();
		endpoint(endpoint_addr&, endpoint_config&);
		endpoint(endpoint&&) noexcept;
		endpoint& operator=(endpoint&&) noexcept;
		~endpoint();
	};
	struct endpoint_list {
		std::map<endpoint_descriptor, endpoint> ep_map;
		std::mutex ep_list_mtx;
		static std::atomic_bool _terminate;
		std::unordered_map<u32, std::thread> worker_threads;
		std::queue<u32> scheduled_endpoint;
		std::mutex consumer_mtx;
		std::mutex producer_mtx;
		std::condition_variable cv_consumer;
		std::condition_variable cv_producer;

		std::vector<std::thread> stream_worker_threads;
		std::queue<std::function<void()>> scheduled_stream_work;
		std::mutex schedule_stream_work_mtx;

		std::vector<std::thread> _time_quantum_threads;

		std::condition_variable cv_stream_work_consumer;
		std::condition_variable cv_stream_work_producer;

		static void schedule_stream_work(std::function<void()>);
		static void stream_work_pooling();
		static void endpoint_single_worker_thread_wrapper(endpoint_list&);

		endpoint_list();
		~endpoint_list();
	};

	extern endpoint_list ep_list;

	u64 polynomial_div(u64 n, u64 divisor);
	u64 compute_crc(u64 d, u64 g);
	u64 compute_crc(const char* ptr, u64 size, u64 g);

#ifdef SIMPLE_UDP_FRAMEWORK_IMPL
	sockaddr endpoint_addr_to_sockaddr(endpoint_addr&);
	endpoint_addr sockaddr_to_endpoint_addr(sockaddr&);
#endif
	endpoint_descriptor endpoint_addr_to_endpoint_descriptor(endpoint_addr&);

	endpoint& get_endpoint(endpoint_addr&);
	connection& get_connection(endpoint_addr& local, endpoint_addr& remote);
	void open_endpoint(endpoint_addr&, endpoint_config&);
	void open_connection(endpoint_addr& local, endpoint_addr& remote, connection_config&);
	void close_endpoint(endpoint_addr& local);
	void close_connection(endpoint_addr& local, endpoint_addr& remote);
	std::optional<connection*> connect(endpoint_addr& local, endpoint_addr& remote, endpoint_config&, connection_config&);
}

namespace udpf_interface { //optional: todo: create a simple interface or just touch the guts directly

	using connection_config = udpf_impl::connection_config;
	using endpoint_connection_request = udpf_impl::listener::endpoint_connection_request;
	using endpoint_addr_in = udpf_impl::endpoint_addr;
	using u16 = udpf_impl::u16;
	using u32 = udpf_impl::u32;
	using u64 = udpf_impl::u64;

	struct endpoint_addr;

	namespace implementation {
		udpf_impl::endpoint_addr convert_endpoint_addr_interface_to_impl(endpoint_addr&);
	}

	struct endpoint_addr {
	private:
		udpf_impl::endpoint_addr ep_addr;
	public:
		std::string get_ip_port_string();
		std::string get_ip_string();
		std::string get_port_string();
		u16 get_port();
		u16 get_net_port();
		u32 get_net_ip();

		endpoint_addr(endpoint_addr_in);

		friend struct listener;
		friend struct connection;
		friend udpf_impl::endpoint_addr implementation::convert_endpoint_addr_interface_to_impl(endpoint_addr&);
	};

	struct connection {
		endpoint_addr local_addr;
		endpoint_addr peer_addr;

		int send(void* buff, u64 size);
		std::unique_ptr<char> recv();
		void close();
	};

	struct listener {
		endpoint_addr local_addr;
		connection_config con_config;

		std::optional<endpoint_connection_request> listen();
		connection accept(endpoint_connection_request);
		void close();

		listener(endpoint_addr_in);
	};

	std::optional<udpf_interface::connection>  connect(endpoint_addr_in& local, endpoint_addr_in& peer, connection_config&);
	endpoint_addr_in endpoint_addr_info(u16 port, u32 netip = 0); //default is local ip
	endpoint_addr_in endpoint_addr_info(u16 port, std::string const& ip_string); 
}

#ifdef SIMPLE_UDP_FRAMEWORK_IMPL
using namespace udpf_impl;
#endif

#endif