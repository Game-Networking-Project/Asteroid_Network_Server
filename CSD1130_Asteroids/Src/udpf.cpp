#define SIMPLE_UDP_FRAMEWORK_IMPL
#include "udpf.h"
#include <bit>
#include <cassert>

endpoint_list udpf_impl::ep_list{};
std::atomic_bool udpf_impl::endpoint_list::_terminate{ false };

bool udpf_impl::endpoint::recv()
{
	sockaddr src;
	int sz_src{ sizeof(sockaddr) };
	int p{ ::recvfrom(sfd, ef_buffer, endpoint_frame::frame_size, MSG_PEEK, &src, &sz_src) };
	if (p == 0)
		return false;
	if (p < 0) {
		int ec{ WSAGetLastError() };
		if (ec == WSAEWOULDBLOCK)
			return true;
		if (ec != WSAEMSGSIZE)
			return false;
	}
	endpoint_frame eframe{ ef_buffer, endpoint_frame::frame_size };
	if (eframe.is_corrupt())
		discard();
	if (eframe.type == eframe.LISTENER_FRAME) {
		std::lock_guard<std::mutex> llm{ lr.listener_mtx };
		if (lr.item_count == 0 || lr.lr_state != lr.CLOSED) {
			lr.item_count++;
			::recv(sfd, lr.lf_buffer, listener_frame::transport_frame_size, 0);
		}
		else
			discard();
		lr.cv.notify_one();
	}
	else if (eframe.type == eframe.CONNECTION_FRAME) {
		{
			endpoint_addr ep_addr{ sockaddr_to_endpoint_addr(src) };
			endpoint_descriptor ep_desc{ endpoint_addr_to_endpoint_descriptor(ep_addr) };
			std::lock_guard<std::mutex> lm{ recv_mtx };
			if (cn.find(ep_desc) == cn.end()) {
				discard();
				return true;
			}
			connection& cref{ cn[ep_desc] };
			std::scoped_lock sclock{ cref.cn_write_mtx, cref.cn_mtx };
			//int dgram_size{ ::recv(sfd, cref.buffer, cref.config.max_datagram_size, 0) };
			if (cref.buffer_item_count == 0) {
				//::recv(sfd, cref.buffer, cref.config.max_datagram_size, 0);
				::recv(sfd, cref.buffer, (cref.cn_state == cref.WAIT_FOR_INIT) ? listener_frame::transport_frame_size + connection_frame::transport_frame_header_size : cref.config.max_datagram_size, 0);
				cref.buffer_item_count++;
			}
			else {
				discard();
			}
			cref.cv_read_buffer.notify_one();
		}
	}
	else if (eframe.type == eframe.STREAM_FRAME) {
		constexpr int peek_streamid_size{ endpoint_frame::frame_size + sizeof(u32) + sizeof(u16) };
		constexpr int peek_streamid_pos{ endpoint_frame::frame_size + sizeof(u16) };
		char process_buffer[peek_streamid_size]{}; //enough to peek the stream id (crc16 bits  + streamid32 bits)
		::recvfrom(sfd, process_buffer, peek_streamid_size, MSG_PEEK, &src, &sz_src);
		u32 stream_id{ ntohl(*(u32*)(process_buffer + peek_streamid_pos)) }; //no crc check here just trust

		endpoint_addr ep_addr{ sockaddr_to_endpoint_addr(src) };
		endpoint_descriptor ep_desc{ endpoint_addr_to_endpoint_descriptor(ep_addr) };
		std::lock_guard<std::mutex> lm{ recv_mtx };
		if (cn.find(ep_desc) == cn.end() || stream_id > 1) { //discard stream id above 1. current implementation only support 2 hardcoded streams 0 for write, 1 for read
			discard();
			return true;
		}
		stream& sref{ cn[ep_desc].streams[stream_id] };
		std::scoped_lock sclock{ sref.s_producer_mtx, sref.s_buffer_mtx, sref.s_mtx };
		//int dgram_size{ ::recv(sfd, cref.buffer, cref.config.max_datagram_size, 0) };
		if (sref.buffer_item_count == 0) {
			//::recv(sfd, cref.buffer, cref.config.max_datagram_size, 0);
			::recv(sfd, sref.buffer, sref.conn->config.max_datagram_size, 0);
			sref.buffer_item_count++;
		}
		else {
			discard();
		}
		endpoint_list::schedule_stream_work(std::bind(&stream::process_incoming, &sref));
		//sref.cv_consumer.notify_one();
	}
	else
		discard();
	return true;
}

void udpf_impl::endpoint::discard()
{
	::recv(sfd, ef_buffer, endpoint_frame::frame_size, 0);
}

void udpf_impl::endpoint::send(frame_ptr frame, endpoint_addr& dest, u64 size)
{
	std::lock_guard send_lock{ send_mtx };
	sockaddr dst{ endpoint_addr_to_sockaddr(dest) };
	sendto(sfd, frame, size, 0, &dst, sizeof(sockaddr));
}

void udpf_impl::endpoint::close()
{
	lr.close();
	for (auto& cpair : cn) {
		cpair.second.close();
	}
}

udpf_impl::endpoint::endpoint()
	: sfd{ socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) }
{
}

udpf_impl::endpoint::endpoint(endpoint_addr& la, endpoint_config& ecfg)
	: sfd{ socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) }, local{ la }, config{ ecfg }
{
	sockaddr saddr{ endpoint_addr_to_sockaddr(la) };
	int ec{ bind(sfd, &saddr, sizeof(sockaddr)) };
	assert(ec >= 0 && "error code" && std::to_string(ec).c_str() && "endpoint binding failed");

	ULONG true_val{ 1 };
	ioctlsocket(sfd, FIONBIO, &true_val);

	lr.local = local;
}

udpf_impl::endpoint::endpoint(endpoint&& m) noexcept
	: local{ m.local }, config{ m.config }, sfd{ m.sfd }, cn{ std::move(m.cn) }
{
	m.sfd = INVALID_SOCKET;
	lr.local = m.lr.local;
	lr.config = m.lr.config;
	lr.item_count = m.lr.item_count;
	lr.lr_state = m.lr.lr_state;
	memcpy(lr.lf_buffer, &m.lr.lf_buffer, listener_frame::frame_size);
}

endpoint& udpf_impl::endpoint::operator=(endpoint&& rhs) noexcept
{
	this->~endpoint();
	new (this) (endpoint){ std::forward<endpoint>(rhs) };
	return *this;
}

udpf_impl::endpoint::~endpoint()
{
	if (sfd != INVALID_SOCKET)
		closesocket(sfd);
}


u64 udpf_impl::polynomial_div(u64 n, u64 div)
{
	int r{ std::bit_width(div) };
	if (r == 0)
		return 0;
	while (std::bit_width(n) >= r) {
		int bit_diff{ std::bit_width(n) - r };
		n ^= div << bit_diff;
	}
	return u64(n);
}

u64 udpf_impl::compute_crc(u64 d, u64 g)
{
	return polynomial_div(polynomial_div(d, g) << (std::bit_width(g) - 1), g);
}

u64 udpf_impl::compute_crc(const char* ptr, u64 size, u64 g)
{
	const char* pend{ ptr + size };
	int r{ std::bit_width(g) };
	u64 r_sum{};
	while (ptr != pend) {
		r_sum <<= 8;
		r_sum |= *(ptr++);
		r_sum = polynomial_div(r_sum, g);
	}
	return compute_crc(r_sum, g);
}

sockaddr udpf_impl::endpoint_addr_to_sockaddr(endpoint_addr& ep_addr) {
	sockaddr res{};
	res.sa_family = AF_INET;
	memcpy(res.sa_data, &ep_addr.netport, sizeof(u16));
	memcpy(res.sa_data + sizeof(u16), &ep_addr.ipv4.netip, sizeof(u32));
	return res;
}

endpoint_addr udpf_impl::sockaddr_to_endpoint_addr(sockaddr& sddr) {
	endpoint_addr ep_adr;
	memcpy(&ep_adr.netport, sddr.sa_data, sizeof(u16));
	memcpy(&ep_adr.ipv4.netip, sddr.sa_data + sizeof(u16), sizeof(u32));
	return ep_adr;
}

inline endpoint_descriptor udpf_impl::endpoint_addr_to_endpoint_descriptor(endpoint_addr& ep_addr) {
	return ep_addr.ipv4.netip << 16 | ep_addr.netport;
}

endpoint& udpf_impl::get_endpoint(endpoint_addr& ep_addr)
{
	//std::lock_guard<std::mutex> lm{ ep_list.ep_list_mtx };
	assert(ep_list.ep_map.find(endpoint_addr_to_endpoint_descriptor(ep_addr)) != ep_list.ep_map.end() && "open endpoint not found.");
	return ep_list.ep_map[endpoint_addr_to_endpoint_descriptor(ep_addr)];
}

connection& udpf_impl::get_connection(endpoint_addr& local, endpoint_addr& remote)
{
	endpoint& ep{ get_endpoint(local) };
	assert(ep.cn.find(endpoint_addr_to_endpoint_descriptor(remote)) != ep.cn.end() && "open endpoint not found.");
	return ep.cn[endpoint_addr_to_endpoint_descriptor(remote)];
}

void udpf_impl::open_endpoint(endpoint_addr& ep_addr, endpoint_config& ep_config) {
	endpoint_descriptor ep_desc{ endpoint_addr_to_endpoint_descriptor(ep_addr) };
	if (ep_list.ep_map.find(ep_desc) == ep_list.ep_map.end()) {
		ep_list.ep_map.emplace(ep_desc, endpoint(ep_addr, ep_config));
	}
}

void udpf_impl::open_connection(endpoint_addr& local, endpoint_addr& remote, connection_config& config)
{
	endpoint& local_ep{ get_endpoint(local) };
	endpoint_descriptor remote_ep_desc{ endpoint_addr_to_endpoint_descriptor(remote) };
	{
		std::lock_guard lm{ local_ep.ep_mtx };
		if (local_ep.cn.find(remote_ep_desc) == local_ep.cn.end()) {
			local_ep.cn.emplace(remote_ep_desc, connection());
			local_ep.cn[remote_ep_desc].local = local;
			local_ep.cn[remote_ep_desc].remote = remote;
			local_ep.cn[remote_ep_desc].cn_state = connection::WAIT_FOR_INIT;
			local_ep.cn[remote_ep_desc].config = config;

			local_ep.cn[remote_ep_desc].buffer = new char[connection_frame::transport_init_handshake_frame_size] {};
			local_ep.cn[remote_ep_desc].read_thread = std::move(std::thread(&connection::read_buffer_thread_wrapper, std::ref(local_ep.cn[remote_ep_desc])));
			local_ep.cn[remote_ep_desc].last_resp_timestamp = std::chrono::steady_clock::now();
		}
	}
}

void udpf_impl::close_endpoint(endpoint_addr& local)
{
	endpoint_descriptor ep_desc{ endpoint_addr_to_endpoint_descriptor(local) };
	if (ep_list.ep_map.find(ep_desc) != ep_list.ep_map.end()) {
		endpoint& ep{ get_endpoint(local) };
		ep.close();
		ep_list.ep_map.erase(ep_desc);
	}
}

void udpf_impl::close_connection(endpoint_addr& local, endpoint_addr& remote)
{
	endpoint_descriptor ep_desc{ endpoint_addr_to_endpoint_descriptor(local) };
	endpoint_descriptor remote_ep_desc{ endpoint_addr_to_endpoint_descriptor(remote) };
	if (ep_list.ep_map.find(ep_desc) != ep_list.ep_map.end()) {
		endpoint& ep{ get_endpoint(local) };
		if (ep.cn.find(remote_ep_desc) != ep.cn.end()) {
			ep.cn[remote_ep_desc].close();
		}
	}
}

std::optional<connection*> udpf_impl::connect(endpoint_addr& local, endpoint_addr& remote, endpoint_config& ecfg, connection_config& ccfg)
{
	open_endpoint(local, ecfg);
	open_connection(local, remote, ccfg);
	endpoint& ep{ get_endpoint(local) };
	listener_frame lframe;
	lframe.in_addr = local;
	lframe.in_config = ccfg;
	lframe.crc_pdiv = 0x9b;
	lframe.type = 0x0;
	auto uptr{ lframe.make_transport_frame() };
	auto tp{ std::chrono::steady_clock::now() };
	u32 elapsed_time_mili{};
	while (get_connection(local, remote).cn_state == connection::WAIT_FOR_INIT && (ccfg.connection_timeout_mili && elapsed_time_mili < ccfg.connection_timeout_mili)) { //if timeout is 0 will not connect timeout
		elapsed_time_mili = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tp).count();
		ep.send(uptr.get(), remote, lframe.transport_frame_size);
	}
	return (!ccfg.connection_timeout_mili || elapsed_time_mili < ccfg.connection_timeout_mili) ? std::make_optional<connection*>(&get_connection(local, remote)) : std::nullopt;
}

void udpf_impl::endpoint_list::endpoint_single_worker_thread_wrapper(endpoint_list& el)
{
	std::queue<endpoint_descriptor> del{};
	while (!_terminate) {
		{
			{
				std::lock_guard<std::mutex> lg{ el.ep_list_mtx };
				for (auto& p : el.ep_map) {
					bool no_error{ p.second.recv() };
					if (!no_error)
						del.push(p.first);
				}
			}
			while (!del.empty()) {
				el.ep_map.erase(del.front());
				del.pop();
			}
		}
	}
}

udpf_impl::endpoint_list::endpoint_list()
{
	WSAData wsa_dt;
	int res;

	// Initialize Winsock
	res = WSAStartup(MAKEWORD(2, 2), &wsa_dt);
	if (res != 0) {
		std::runtime_error("WSAStartup failed: " + std::to_string(res) + "\n");
	}

	worker_threads.emplace(0, std::thread(&endpoint_single_worker_thread_wrapper, std::ref(*this)));
	for (int i{}; i < STREAM_PROCESSING_THREAD_COUNT; i++)
		stream_worker_threads.emplace_back(std::thread(&stream_work_pooling));
}

udpf_impl::endpoint_list::~endpoint_list()
{
	_terminate = true;
	for (auto& ep_pair : worker_threads) {
		ep_pair.second.join();
	}
	WSACleanup();

	cv_stream_work_consumer.notify_all();

	for (auto& strm : stream_worker_threads) {
		strm.join();
	}
}

void udpf_impl::endpoint_list::schedule_stream_work(std::function<void()> fn) {
	std::lock_guard queue_lock{ ep_list.schedule_stream_work_mtx };
	ep_list.scheduled_stream_work.push(fn);
	ep_list.cv_stream_work_consumer.notify_one();
}

void udpf_impl::endpoint_list::stream_work_pooling()
{
	while (!_terminate) {
		std::function<void()> s_work_fn{};
		{
			std::unique_lock queue_lock{ ep_list.schedule_stream_work_mtx };
			ep_list.cv_stream_work_consumer.wait(queue_lock, [&]() {return !ep_list.scheduled_stream_work.empty() || _terminate; });
			if (_terminate)
				continue;
			s_work_fn = ep_list.scheduled_stream_work.front();
			ep_list.scheduled_stream_work.pop();
		}
		s_work_fn();
	}
}

udpf_impl::connection::operator bool()
{
	return cn_state != CONN_ERROR && cn_state != CLOSE;
}

udpf_impl::connection::connection()
	: local{}, remote{ }, config{ }, buffer{ nullptr }, buffer_item_count{ }, store{ nullptr }, buffered_messages{  }, store_size{  }, s_ack{ }, s_base{  }, read_thread{ }, cn_state{ WAIT_FOR_INIT }
{
}

udpf_impl::connection::connection(connection&& m) noexcept
	: local{ m.local }, remote{ m.remote }, config{ m.config }, buffer{ m.buffer }, buffer_item_count{ m.buffer_item_count }, store{ m.store }, buffered_messages{ std::move(m.buffered_messages) }, store_size{ m.store_size }, s_ack{ std::move(m.s_ack) }, s_base{ m.s_base }, read_thread{ std::move(m.read_thread) }, cn_state{ m.cn_state }
{
	m.store = nullptr;
	m.buffer = nullptr;
}

udpf_impl::connection::~connection()
{
	close();
	if (store)
		delete[] store;
	if (buffer)
		delete[] buffer;
}

connection& udpf_impl::connection::operator=(connection&& rhs) noexcept
{
	this->~connection();
	new (this) (connection){ std::forward<connection>(rhs) };
	return *this;
}

void udpf_impl::connection::read_buffer()
{
	bool _sleep{};
	{
		std::lock_guard<std::mutex> lm{ cn_mtx };
		connection_frame cframe{ buffer, config.max_datagram_size };
		buffer_item_count--;

		if (cframe.is_corrupt())
			return;
		if (cframe.is_init_handshake() && cn_state == WAIT_FOR_INIT) {		//we reuse listener frame cos i lazy format -> {endpoint frame}[connection frame | {listener frame}] payload == listener frame for INIT_HANDSHAKE msg
			listener_frame lframe{ cframe.payload, listener_frame::frame_size };			//payload is negotiated connection configurations
			init(lframe.in_config);
		}
		else if (cframe.is_fin() || cframe.is_cerror()) {
			cn_state = CLOSE;	//close on error or fin msg
		}
		else if (cn_state != WAIT_FOR_INIT) {
			if (cframe.is_bfull()) {
				_sleep = true;
			}
			else if (cframe.is_data() && cn_state == RECV) {

			}
			else if (cframe.is_ping()) {
				if (!cframe.is_ack()) { //is ping only, then send ping ack
					cframe.type |= cframe.ACK;
					auto uptr{ cframe.make_transport_frame() };
					get_endpoint(local).send(uptr.get(), remote, cframe.transport_frame_header_size + cframe.payload_len);
				}
			}
		}
		last_resp_timestamp = std::chrono::steady_clock::now();
	}
	if (_sleep) {
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(200ms);											//hardcode sleep if recv buffer is full for flow control
	}
}

#include <iostream>

void udpf_impl::connection::read_buffer_thread_wrapper(connection& cn)
{
	int ping_count{};
	while (cn) {
		u32 ping_interval{ cn.config.connection_timeout_mili / PING_COUNT };
		std::unique_lock<std::mutex> ul{ cn.cn_mtx };
		using namespace std::chrono_literals;
		cn.cv_read_buffer.wait_for(ul, 1s, [&]() {return cn.buffer_item_count > 0 || !cn; }); //wait 1s for new notification or termination
		//cn.cv_read_buffer.wait(ul, [&]() {return cn.buffer_item_count > 0 || !cn; }); //wait for new notification or termination
		std::chrono::milliseconds time_diff{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - cn.last_resp_timestamp) };
		if (!cn || time_diff.count() >= cn.config.connection_timeout_mili)
			continue;
			//break;
		ul.unlock();	//release lock for read_buffer
		if (cn.buffer_item_count > 0) {
			cn.read_buffer();
			ping_count = 0;
		}
		else if (time_diff.count() >= ping_interval * (ping_count + 1)) {
			ping_count++;
			cn.send_ping();
		}
	}
	std::cout << "thread closed";
}

void udpf_impl::connection::send_init_req(connection_config& ccfg)
{
	endpoint& ep{ get_endpoint(local) };
	listener_frame lframe{};
	lframe.crc_pdiv = 0xac;
	lframe.in_addr = local;
	lframe.in_config = ccfg;
	lframe.type = 0;
	auto uptr{ lframe.make_transport_frame() };
	connection_frame cframe{};
	cframe.type = cframe.INIT_HANDSHAKE;
	cframe.crc_pdiv = 0xacdc;
	cframe.payload_len = listener_frame::transport_frame_size;
	cframe.payload = uptr.get();
	auto ucptr{ cframe.make_transport_frame() };
	cn_state = READY;

	ep.send(ucptr.get(), remote, connection_frame::transport_frame_header_size + cframe.payload_len);
}

void udpf_impl::connection::send_ping()
{
	connection_frame cframe{};
	cframe.type = cframe.PING;
	cframe.crc_pdiv = 0xacdc;
	cframe.payload_len = 0;
	cframe.payload = nullptr;
	auto ucptr{ cframe.make_transport_frame() };
	endpoint& ep{ get_endpoint(local) };
	ep.send(ucptr.get(), remote, connection_frame::transport_frame_header_size + cframe.payload_len);
}


void udpf_impl::connection::init(connection_config& ccfg)
{
	if (cn_state != WAIT_FOR_INIT)
		return;
	if (buffer)
		delete[] buffer;
	buffer = new char[ccfg.max_datagram_size] {};
	config = ccfg;
	streams.push_back(stream());
	streams.push_back(stream());
	streams[stream::READ_STREAM].conn = this;
	streams[stream::WRITE_STREAM].conn = this;
	streams[stream::READ_STREAM].reset(ccfg);
	streams[stream::WRITE_STREAM].reset(ccfg);

	cn_state = READY;
	//read_thread = std::move(std::thread(&read_buffer_thread_wrapper, std::ref(*this)));
}

void udpf_impl::connection::close()
{
	{
		std::scoped_lock sl{ cn_mtx, cn_read_mtx, cn_write_mtx };
		cn_state = CLOSE;
		cv_write.notify_all();
		cv_read.notify_all();
		cv_read_buffer.notify_all();
	}
	if (read_thread.joinable())
		read_thread.join();
}

udpf_impl::endpoint_frame::endpoint_frame()
{
}

udpf_impl::endpoint_frame::endpoint_frame(frame_ptr frptr, u32 f_size)
{
	assert(f_size >= frame_size && "invalid read. frame size too small");
	crc8_cs = *frptr;
	type = *(frptr + 1);
	crc_pdiv = *(frptr + 2);
	_padding = *(frptr + 3);
}

bool udpf_impl::endpoint_frame::is_corrupt()
{
	return compute_crc((type << 16) | (crc_pdiv << 8) | _padding, crc_pdiv) != crc8_cs;
}

void udpf_impl::endpoint_frame::flush_to_buffer(char* buffer, u64 size)
{
	assert(size >= frame_size && "input buffer size too small");
	*(buffer + 1) = type;
	*(buffer + 2) = crc_pdiv;
	*(buffer + 3) = _padding;
	*buffer = crc8_cs;
}

udpf_impl::listener_frame::listener_frame()
{
}

udpf_impl::listener_frame::listener_frame(frame_ptr frptr, u32 fsz)
	: crc8_cs{}, type{}, crc_pdiv{}, in_addr{}, in_config{}
{
	assert(fsz <= transport_frame_size && "invalid read. frame size too small");
	frptr += endpoint_frame::frame_size;
	crc8_cs = *frptr;
	type = *(frptr + 1);
	crc_pdiv = *(frptr + 2);
	memcpy(&in_addr.ipv4.netip, frptr + 3, sizeof(u32));
	memcpy(&in_addr.netport, frptr + 7, sizeof(u16));
	memcpy(&in_config.max_datagram_size, frptr + 9, sizeof(u16));
	memcpy(&in_config.max_window_size, frptr + 11, sizeof(u16));
	memcpy(&in_config.connection_timeout_mili, frptr + 13, sizeof(u32));
	memcpy(&in_config.packet_timeout_mili, frptr + 17, sizeof(u32));

	in_config.max_datagram_size = ntohs(in_config.max_datagram_size);
	in_config.max_window_size = ntohs(in_config.max_window_size);
	in_config.connection_timeout_mili = ntohl(in_config.connection_timeout_mili);
	in_config.packet_timeout_mili = ntohl(in_config.packet_timeout_mili);
}

bool udpf_impl::listener_frame::is_corrupt()
{
	char buffer[frame_size];
	flush_to_buffer(buffer, frame_size);
	return compute_crc(buffer + 1, frame_size - 1, crc_pdiv) != crc8_cs;
}

void udpf_impl::listener_frame::flush_to_buffer(char* buff, u32 buff_size)
{
	assert(buff_size <= frame_size && "invalid read. frame size too small");
	*buff = crc8_cs;
	*(buff + 1) = type;
	*(buff + 2) = crc_pdiv;
	memcpy(buff + 3, &in_addr.ipv4.netip, sizeof(u32));
	memcpy(buff + 7, &in_addr.netport, sizeof(u16));

	u16 netdgram{ htons(in_config.max_datagram_size) };
	u16 netwnd{ htons(in_config.max_window_size) };
	u32 netctime{ htonl(in_config.connection_timeout_mili) };
	u32 netptime{ htonl(in_config.packet_timeout_mili) };

	memcpy(buff + 9, &netdgram, sizeof(u16));
	memcpy(buff + 11, &netwnd, sizeof(u16));
	memcpy(buff + 13, &netctime, sizeof(u32));
	memcpy(buff + 17, &netptime, sizeof(u32));
}

std::unique_ptr<char> udpf_impl::listener_frame::make_transport_frame()
{
	char* nptr{ new char[transport_frame_size] };
	endpoint_frame eframe;
	eframe.crc_pdiv = 0xa2;
	eframe.type = endpoint_frame::LISTENER_FRAME;
	eframe._padding = 0x0;
	eframe.crc8_cs = compute_crc((eframe.type << 16) | (eframe.crc_pdiv << 8) | eframe._padding, eframe.crc_pdiv);
	eframe.flush_to_buffer(nptr, transport_frame_size);

	char* wptr{ nptr + eframe.frame_size };
	flush_to_buffer(wptr, transport_frame_size - eframe.frame_size);
	*wptr = compute_crc(wptr + 1, frame_size - 1, crc_pdiv);

	return std::unique_ptr<char>(nptr);
}

udpf_impl::connection_frame::connection_frame()
{
}

udpf_impl::connection_frame::connection_frame(frame_ptr frptr, u32 fsz)
{
	assert(fsz >= transport_frame_header_size && "invalid read. frame size too small");
	frptr += endpoint_frame::frame_size;
	memcpy(&crc16_cs, frptr, sizeof(u16));
	type = *(frptr + 2);
	memcpy(&seq_num, frptr + 3, sizeof(u32));
	memcpy(&crc_pdiv, frptr + 7, sizeof(u16));
	memcpy(&payload_len, frptr + 9, sizeof(u32));
	crc16_cs = ntohs(crc16_cs);
	seq_num = ntohl(seq_num);
	crc_pdiv = ntohs(crc_pdiv);
	payload_len = ntohs(payload_len);
	payload = frptr + frame_header_size;
}

inline bool udpf_impl::connection_frame::is_corrupt()
{
	return compute_crc(payload + 2 - frame_header_size, payload_len - 2 + frame_header_size, crc_pdiv) != crc16_cs;
}

inline bool udpf_impl::connection_frame::is_init_handshake()
{
	return !is_data();
}

inline bool udpf_impl::connection_frame::is_ping()
{
	return type & PING;
}

inline bool udpf_impl::connection_frame::is_ack()
{
	return type & ACK;
}

inline bool udpf_impl::connection_frame::is_fin()
{
	return type & FIN;
}

inline bool udpf_impl::connection_frame::is_bfull()
{
	return type & BUFFER_FULL;
}

inline bool udpf_impl::connection_frame::is_cerror()
{
	return type & CONN_ERROR;
}

inline bool udpf_impl::connection_frame::is_data()
{
	return type & DATA;
}

std::unique_ptr<char> udpf_impl::connection_frame::make_transport_frame()
{
	char* nptr{ new char[transport_frame_header_size + payload_len] {} };
	endpoint_frame eframe;
	eframe.crc_pdiv = 0xa2;
	eframe.type = endpoint_frame::CONNECTION_FRAME;
	eframe._padding = 0x0;
	eframe.crc8_cs = compute_crc((eframe.type << 16) | (eframe.crc_pdiv << 8) | eframe._padding, eframe.crc_pdiv);
	eframe.flush_to_buffer(nptr, transport_frame_header_size);

	char* wptr{ nptr + eframe.frame_size };
	u32 netseq_num = htonl(seq_num);
	u16 netcrc_pdiv = htons(crc_pdiv);
	u16 netpayload_len = htons(payload_len);
	*(wptr + 2) = type;
	memcpy(wptr + 3, &netseq_num, sizeof(u32));
	memcpy(wptr + 7, &netcrc_pdiv, sizeof(u16));
	memcpy(wptr + 9, &netpayload_len, sizeof(u16));

	memcpy(wptr + 11, payload, payload_len);

	u16 netcrc16_cs = htons(compute_crc(wptr + 2, payload_len - 2 + frame_header_size, crc_pdiv));
	memcpy(wptr, &netcrc16_cs, sizeof(u16));

	return std::unique_ptr<char>(nptr);
}

void udpf_impl::listener::close()
{
	{
		std::lock_guard lm{ listener_mtx };
		lr_state = CLOSED;
		cv.notify_all();
	}
}

std::optional<udpf_impl::listener::endpoint_connection_request> udpf_impl::listener::listen()
{
	std::optional<endpoint_connection_request> incoming{ std::nullopt };
	//u32 req_timeout{ config.request_timeout_mili };
	std::unique_lock ulock{ listener_mtx };
	cv.wait(ulock);
	item_count--;

	listener_frame lframe{ lf_buffer, listener_frame::transport_frame_size };
	if (!lframe.is_corrupt()) {
		incoming = std::make_optional<endpoint_connection_request>({ lframe.in_addr, lframe.in_config });
	}

	return incoming;
}

connection& udpf_impl::listener::accept(endpoint_connection_request& ecreq)
{
	open_connection(local, ecreq.first, ecreq.second);
	connection& cn{ get_connection(local, ecreq.first) };
	cn.init(ecreq.second);
	cn.send_init_req(ecreq.second);
	return cn;
}

udpf_impl::stream_frame::stream_frame()
{
}

udpf_impl::stream_frame::stream_frame(frame_ptr frptr, u32 fsz)
{
	assert(fsz >= transport_frame_header_size && "invalid read. frame size too small");
	frptr += endpoint_frame::frame_size;
	memcpy(&crc16_cs, frptr, sizeof(u16));
	memcpy(&stream_id, frptr + sizeof(u16), sizeof(u32));
	frptr += sizeof(u32);	//here cos i copied the code from connection_frame and is too lazy to change
	type = *(frptr + 2);
	memcpy(&seq_num, frptr + 3, sizeof(u32));
	memcpy(&crc_pdiv, frptr + 7, sizeof(u16));
	memcpy(&payload_len, frptr + 9, sizeof(u16));
	crc16_cs = ntohs(crc16_cs);
	stream_id = ntohl(stream_id);
	seq_num = ntohl(seq_num);
	crc_pdiv = ntohs(crc_pdiv);
	payload_len = ntohs(payload_len);
	frptr -= sizeof(u32);	//same rationale here
	payload = frptr + frame_header_size;
}

bool udpf_impl::stream_frame::is_corrupt()
{
	return compute_crc(payload + sizeof(u16) - frame_header_size, payload_len - sizeof(u16) + frame_header_size, crc_pdiv) != crc16_cs; //skip crc checksum
}

inline bool udpf_impl::stream_frame::is_data()
{
	return !is_ack();
}

inline bool udpf_impl::stream_frame::is_ack()
{
	return type & ACK;
}

inline bool udpf_impl::stream_frame::is_syn()
{
	return type & SYN;
}

inline bool udpf_impl::stream_frame::is_fin()
{
	return type & FIN;
}

std::unique_ptr<char> udpf_impl::stream_frame::make_transport_frame()
{
	char* nptr{ new char[transport_frame_header_size + payload_len] {} };
	endpoint_frame eframe;
	eframe.crc_pdiv = 0xa2;
	eframe.type = endpoint_frame::STREAM_FRAME;
	eframe._padding = 0x0;
	eframe.crc8_cs = compute_crc((eframe.type << 16) | (eframe.crc_pdiv << 8) | eframe._padding, eframe.crc_pdiv);
	eframe.flush_to_buffer(nptr, transport_frame_header_size);

	char* wptr{ nptr + eframe.frame_size };
	u32 netseq_num = htonl(seq_num);
	u16 netcrc_pdiv = htons(crc_pdiv);
	u32 netstream_id = htonl(stream_id);
	u16 netpayload_len = htons(payload_len);

	memcpy(wptr + 2, &netstream_id, sizeof(u32));
	*(wptr + 6) = type;
	memcpy(wptr + 7, &netseq_num, sizeof(u32));
	memcpy(wptr + 11, &netcrc_pdiv, sizeof(u16));
	memcpy(wptr + 13, &netpayload_len, sizeof(u16));

	memcpy(wptr + 15, payload, payload_len);

	u16 netcrc16_cs = htons(compute_crc(wptr + 2, payload_len - 2 + frame_header_size, crc_pdiv));
	memcpy(wptr, &netcrc16_cs, sizeof(u16));

	return std::unique_ptr<char>(nptr);
}

std::unique_ptr<char> udpf_impl::stream_frame::make_syn_transport_frame(u32 message_size)
{
	payload_len = sizeof(u32);
	message_size = htonl(message_size);
	payload = (char*)(&message_size);
	return make_transport_frame();
}

void udpf_impl::stream::process_incoming_stream_frame_to_store(stream_frame& isf) {
	memcpy(s_store + isf.seq_num, isf.payload, isf.payload_len);
}

void udpf_impl::stream::process_incoming() {
	std::lock_guard lg{ s_buffer_mtx };
	stream_frame isf{ buffer, conn->config.max_datagram_size };
	u32 peer_stream_id_complement{isf.stream_id^1}; //this is only here cos streams are hardcoded id0 write, id1 read 
	const i64 max_dgram_payload_size{ conn->config.max_datagram_size - stream_frame::transport_frame_header_size };
	const u16 recv_wnd{ conn->config.max_window_size };
	{
		std::lock_guard lg{ s_mtx };
		buffer_item_count = 0;
	}
	if (isf.is_corrupt())
		return;
	if (s_state == RECV) {			
		if (isf.is_data()) {
			i64 seq_diff{ i64(isf.seq_num) - i64(s_base) };
			i64 seq_wbuff{ i64(max_dgram_payload_size) * i64(recv_wnd) };
			if (isf.seq_num == s_base) {
				s_ack[s_ack_ipos] = true;
				move_sliding_window_to_last_unack();
				process_incoming_stream_frame_to_store(isf);
			}
			else if (seq_diff > 0 && seq_diff < seq_wbuff) {
				ack(isf.seq_num);
				process_incoming_stream_frame_to_store(isf);
			}
		}
		else if (isf.is_fin()) {
			if (isf.is_ack()) {	//if fin ack
				std::lock_guard lg{ s_mtx };
				s_state = READY; //if peer has received fin ack and transition to READY successfully this fin ack would be echod back. so pretty safe to transition to ready as peer is not dangling in SEND mode
			}
			else {
				std::lock_guard lg{ s_mtx };
				s_state = CLOSE;
				ep_list.schedule_stream_work(std::bind(&stream::process_send_ack, this, stream_frame::ACK, WRITE_STREAM, 0)); //send ack back
			}
			return;
		}
		{
			std::lock_guard lg{ s_queue_mtx };
			if (isf.seq_num <= s_store_size) {
				seq_sent_queue.push({ isf.seq_num, std::nullopt });
				ep_list.schedule_stream_work(std::bind(&stream::process_outgoing, this));
			}
		}
		if (s_base >= s_store_size) {
			//std::unique_lock prod_lk{ s_producer_mtx };
			//cv_producer.wait()								//dont wait here. this will stall the threadpool
			if (buffered_msg_count < MSG_BUFFER_MAX) {
				std::scoped_lock sl{ s_consumer_mtx, s_producer_mtx };
				buffered_messages.push(std::unique_ptr<char>(s_store));
				buffered_msg_count++;
				s_store = nullptr;
				{
					std::lock_guard lg{ s_mtx };
					s_state = READY;
				}
				cv_consumer.notify_one();
			}
			else {
				ep_list.schedule_stream_work(std::bind(&stream::process_flush_store_to_buffered, this));
			}
		}
		if (isf.is_syn()) {
			seq_sent_queue = {};
			ep_list.schedule_stream_work(std::bind(&stream::process_outgoing, this));
			return;
		}
	}
	else if (s_state == SEND) {
		if (isf.is_ack()) {
			if (isf.is_syn())
				return;
			if (isf.is_fin()) {
				{
					std::lock_guard lg{ s_mtx };
					s_state = READY;
				}
				ep_list.schedule_stream_work(std::bind(&stream::process_send_ack, this, stream_frame::FIN | stream_frame::ACK, WRITE_STREAM, 0)); //forward fin ack back
				return;
			}
			i64 seq_diff{ i64(isf.seq_num) - i64(s_base) };
			i64 seq_wbuff{ i64(max_dgram_payload_size) * i64(recv_wnd) };
			if (isf.seq_num == s_base) {
				std::lock_guard s_ack_lock{ s_ack_mtx };
				s_ack[s_ack_ipos] = true;
				move_sliding_window_to_last_unack();
			}
			else if (seq_diff > 0 && seq_diff < seq_wbuff) {
				std::lock_guard s_ack_lock{ s_ack_mtx };
				ack(isf.seq_num);
			}
		}
	}
	else if (s_state == READY) {	//ready means stream is open for new syn request. which means old data frame coming from peer dangling send should be replied with a fin ack
		if (isf.is_syn()) {
			if (!isf.is_ack()) { //if syn only means recv
				{
					std::lock_guard lg{ s_consumer_mtx };
					if (buffered_msg_count >= MSG_BUFFER_MAX)	//reject if buffer full
						return;
				}
				stream_frame::syn_frame synframe{ buffer, u32(stream_frame::transport_frame_header_size + isf.payload_len) };
				{
					std::lock_guard lg{ s_mtx };
					s_state = RECV;
				}
				s_seek = s_base ^= s_base;
				s_store_size = synframe.total_message_size;
				if (s_store)
					delete[] s_store;
				s_store = new char[s_store_size];
				seq_sent_queue = {};
				ep_list.schedule_stream_work(std::bind(&stream::process_outgoing, this));
			}
			else {	//means resp from sender
				std::lock_guard lg{ s_mtx };
				s_state = SEND;
			}
		}
		else if (isf.is_fin()) {
			//if (isf.is_ack()) {	//fin ack for peer stream state transition from recv -> ready
			//	ep_list.schedule_stream_work(std::bind(&stream::process_send_ack, this, stream_frame::FIN | stream_frame::ACK, peer_stream_id_complement, 0)); //forward fin ack back
			//}
			if (!isf.is_ack()) {
				{
					std::lock_guard lg{ s_mtx };
					s_state = CLOSE; //pure fin is stream termination
				}
				ep_list.schedule_stream_work(std::bind(&stream::process_send_ack, this, stream_frame::ACK, peer_stream_id_complement, 0)); //send ack back
			}
		}
		else if (isf.is_data()) {
			ep_list.schedule_stream_work(std::bind(&stream::process_send_ack, this, stream_frame::FIN | stream_frame::ACK, WRITE_STREAM, 0)); //send fin ack back to inform the sender that stream is not in recv state
		}
	}
	else if (s_state == CLOSE) {
		if (isf.is_fin()) { //already closed so ack back
			ep_list.schedule_stream_work(std::bind(&stream::process_send_ack, this, stream_frame::ACK, peer_stream_id_complement, 0)); //send ack back
		}
	}
}

void udpf_impl::stream::process_outgoing() {
	stream_frame osf{};
	const i64 max_dgram_payload_size{ conn->config.max_datagram_size - stream_frame::transport_frame_header_size };
	const u32 packet_timeout_mili{ conn->config.packet_timeout_mili };
	const u16 send_wnd{ conn->config.max_window_size };
	std::unique_lock ul_proc_og{ s_outgoing_mtx };

	using namespace std::chrono_literals;
	if (!ul_proc_og.owns_lock()) {	//lock fails here if other threads are processing this stream
		std::cv_status cstatus{ cv_outgoing.wait_for(ul_proc_og, 1s, [&]() {return ul_proc_og.owns_lock(); }) };
		if (cstatus == std::cv_status::timeout)
			return;					//drop work since thread is being held by others for too long. no need push work since other thread would push anyways
	}

	if (s_state == RECV) {
		bool should_synack{};
		{
			std::lock_guard lg{ s_queue_mtx };
			should_synack = seq_sent_queue.empty(); //empty queue means syn ack
		}
		if (should_synack) {
			process_send_ack(stream_frame::SYN | stream_frame::ACK, WRITE_STREAM, 0);
		}
		else if (s_base >= s_store_size) {
			{
				std::lock_guard lg{ s_mtx };
				s_state = READY;
			}
			process_send_ack(stream_frame::FIN | stream_frame::ACK, WRITE_STREAM, 0);
		}
		else {
			osf.type = osf.ACK;
			osf.crc_pdiv = 0xfcb8;
			osf.payload = nullptr;
			osf.payload_len = 0;
			osf.stream_id = WRITE_STREAM;	//sending ack to peer write stream which is the sender stream
			{
				std::lock_guard lg{ s_queue_mtx };
				while (!seq_sent_queue.empty()) {
					osf.seq_num = seq_sent_queue.front().first;
					seq_sent_queue.pop();
					auto uptr{ osf.make_transport_frame() };
					get_endpoint(conn->local).send(uptr.get(), conn->remote, stream_frame::transport_frame_header_size + osf.payload_len);
				}
			}
		}
	}
	else if (s_state == SEND) {
		if (s_base < s_store_size) {
			osf.type = osf.DATA;
			osf.crc_pdiv = 0xb41c;
			osf.stream_id = READ_STREAM;	//sending data to peer read stream which is the receiver stream
			while (s_seek < s_base + send_wnd * max_dgram_payload_size && s_seek < s_store_size) {
				{
					std::lock_guard s_ack_lock{ s_ack_mtx };
					if (is_acked(s_seek)) {
						s_seek += max_dgram_payload_size;
						continue;
					}
				}
				osf.payload_len = (s_store_size - s_seek > max_dgram_payload_size) ? max_dgram_payload_size : s_store_size - s_seek;
				osf.payload = s_store + s_seek;
				osf.seq_num = s_seek;
				auto uptr{ osf.make_transport_frame() };
				get_endpoint(conn->local).send(uptr.get(), conn->remote, stream_frame::transport_frame_header_size + osf.payload_len);

				seq_sent_queue.push({ s_seek, std::chrono::steady_clock::now() });	//no need lock if we assume stream to be simplex. incoming for send mode does not use this queue so it is not contested

				s_seek += max_dgram_payload_size;
			}
			while (!seq_sent_queue.empty()) {
				auto spair{ seq_sent_queue.front() };
				if (spair.first < s_base || is_acked(spair.first)) {						//no need lock cos no write at most just send again
					seq_sent_queue.pop();
					continue;
				}
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - spair.second.value()).count() >= packet_timeout_mili) {
					s_seek = s_base;
					seq_sent_queue = {};
				}
				break;
			}
			endpoint_list::schedule_stream_work(std::bind(&stream::process_outgoing, this));
		}
		else {
			s_store = nullptr;
		}
	}

	cv_outgoing.notify_one();
}

void udpf_impl::stream::process_send_ack(int ack_type, int target_stream, int seq)
{
	stream_frame osf{};
	osf.type = ack_type | stream_frame::ACK;
	osf.crc_pdiv = 0xfcb8;
	osf.seq_num = seq;
	osf.payload = nullptr;
	osf.payload_len = 0;
	osf.stream_id = target_stream;	//sending ack to peer write stream which is the sender stream
	auto uptr{ osf.make_transport_frame() };
	get_endpoint(conn->local).send(uptr.get(), conn->remote, stream_frame::transport_frame_header_size + osf.payload_len);
}

void udpf_impl::stream::process_flush_store_to_buffered()
{
	if (s_base >= s_store_size) {
		//std::scoped_lock sl{ s_consumer_mtx, s_producer_mtx };
		//std::unique_lock prod_lk{ s_producer_mtx };
		//cv_producer.wait()								//dont wait here. this will stall the threadpool
		std::lock_guard lc{s_buffer_mtx};
		if (buffered_msg_count < MSG_BUFFER_MAX) {
			std::scoped_lock sl{ s_consumer_mtx, s_producer_mtx };
			buffered_messages.push(std::unique_ptr<char>(s_store));
			buffered_msg_count++;
			s_store = nullptr;
			s_state = READY;
			cv_consumer.notify_one();
		}
	}
}

inline udpf_impl::stream::operator bool() {
	return s_state != CLOSE && s_state != CONN_ERROR;
}

void udpf_impl::stream::ack(sequence_number n) {
	const i64 max_dgram_payload_size{ conn->config.max_datagram_size - stream_frame::transport_frame_header_size };
	const i64 wind_mod{ conn->config.max_window_size };
	s_ack[(s_ack_ipos + (n - s_base) / max_dgram_payload_size) % wind_mod] = true;
}

bool udpf_impl::stream::is_acked(sequence_number n) {
	const i64 max_dgram_payload_size{ conn->config.max_datagram_size - stream_frame::transport_frame_header_size };
	const i64 wind_mod{ conn->config.max_window_size };
	return s_ack[(s_ack_ipos + (n - s_base) / max_dgram_payload_size) % wind_mod];
}

void udpf_impl::stream::reset(connection_config cn)
{
	if (buffer)
		delete[] buffer;
	if (s_store && s_state == RECV)
		delete[] s_store;
	buffer = new char[cn.max_datagram_size] {};
	s_state = READY;
	s_store = nullptr;
	s_store_size = s_ack_ipos = s_seek = s_base = 0;
	s_ack.resize(cn.max_window_size, false);
}

udpf_impl::stream::stream()
{
}

udpf_impl::stream::stream(stream&& s) noexcept
	: conn{ s.conn }, buffer{ s.buffer }, buffer_item_count{ s.buffer_item_count }, s_base{ s.s_base }, s_seek{ s.s_seek }, s_ack{ std::move(s.s_ack) }, seq_sent_queue{ std::move(s.seq_sent_queue) }, s_ack_ipos{ s.s_ack_ipos }, buffered_messages{ std::move(s.buffered_messages) }, buffered_msg_count{ s.buffered_msg_count }, s_store{ s.s_store }, s_store_size{ s.s_store_size }, s_state{ s.s_state }
{
}

udpf_impl::stream::~stream()
{
	if (buffer) {
		delete[] buffer;
	}
	if (s_store && s_state == RECV)
		delete[] s_store;
}

void udpf_impl::stream::move_sliding_window_to_last_unack() {
	const i64 max_dgram_payload_size{ conn->config.max_datagram_size - stream_frame::transport_frame_header_size };
	const i64 wind_mod{ conn->config.max_window_size };
	while (s_ack[s_ack_ipos]) {
		s_ack[s_ack_ipos] = false;
		s_ack_ipos = (s_ack_ipos + 1) % wind_mod;
		s_base += max_dgram_payload_size;
	}
}

void stream::process_stream() {

}

void stream::stream_processing_thread_wrapper(stream& stm) {
	while (stm) {
		{
			std::unique_lock usl{ stm.s_consumer_mtx };
			stm.cv_consumer.wait(usl, [&]() {return stm.buffer_item_count > 0 || !stm; });
			if (!stm)
				break;
		}
		stm.process_stream();
	}
}

udpf_impl::stream_frame::syn_frame::syn_frame()
{
}

udpf_impl::stream_frame::syn_frame::syn_frame(frame_ptr frptr, u32 fsz)
{
	assert(fsz >= transport_frame_size && "invalid read. frame size too small");
	frptr += stream_frame::transport_frame_header_size;
	memcpy(&total_message_size, frptr, sizeof(u32));
	total_message_size = ntohl(total_message_size);
}

void udpf_impl::connection::send(char* data, u64 size) {
	stream& sstream{ streams[stream::WRITE_STREAM] };
	while (cn_state == WAIT_FOR_INIT || (sstream.s_state == sstream.SEND || sstream.s_state == sstream.RECV));

	stream_frame osf{};
	const i64 max_dgram_payload_size{ config.max_datagram_size - stream_frame::transport_frame_header_size };
	const u32 packet_timeout_mili{ config.packet_timeout_mili };
	osf.type = osf.SYN;
	osf.crc_pdiv = 0xb41c;
	osf.stream_id = stream::READ_STREAM;	//sending data to peer read stream which is the receiver stream
	auto syn_ptr{ osf.make_syn_transport_frame(size) };
	{
		std::lock_guard smt(sstream.s_mtx);
		sstream.s_store = data;
		std::fill(sstream.s_ack.begin(), sstream.s_ack.end(), false);
		sstream.s_ack_ipos = 0;
		sstream.buffer_item_count = 0;
		sstream.s_store_size = size;
		sstream.s_seek = sstream.s_base ^= sstream.s_base;
	}
	/*if (sstream.s_store)
		delete[] sstream.s_store;
	sstream.s_store = new char[size] {};*/
	//sstream.s_store = data;
	while (sstream.s_state == stream::READY) {
		get_endpoint(local).send(syn_ptr.get(), remote, stream_frame::syn_frame::transport_frame_size);
	}
	sstream.s_state = stream::SEND;
	endpoint_list::schedule_stream_work(std::bind(&stream::process_outgoing, &sstream));
}

int udpf_interface::connection::send(void* buff, u64 size)
{
	udpf_impl::connection& conn{ udpf_impl::get_connection(local_addr.ep_addr, peer_addr.ep_addr) };
	conn.send((char*)(buff), size);
	while (conn.streams[udpf_impl::stream::WRITE_STREAM].s_state == udpf_impl::stream::SEND);
	return conn.streams[udpf_impl::stream::WRITE_STREAM].s_state == udpf_impl::stream::READY; //should lock here
}

std::unique_ptr<char> udpf_interface::connection::recv() {
	return udpf_impl::get_connection(local_addr.ep_addr, peer_addr.ep_addr).recv();
}

std::unique_ptr<char> udpf_impl::connection::recv() {
	stream& sstream{ streams[stream::READ_STREAM] };
	std::unique_lock uk{ sstream.s_consumer_mtx };
	if (!(uk.owns_lock() && sstream.buffered_msg_count > 0)) {
		sstream.cv_consumer.wait(uk, [&]() {return sstream.buffered_msg_count > 0; });
	}
	std::unique_ptr uptr{ std::move(sstream.buffered_messages.front()) };
	sstream.buffered_msg_count--;
	sstream.buffered_messages.pop();
	return uptr;
}

void udpf_interface::connection::close() {
	udpf_impl::connection& conn{ udpf_impl::get_connection(local_addr.ep_addr, peer_addr.ep_addr) };
	conn.close();
}

std::string udpf_interface::endpoint_addr::get_ip_port_string()
{
	return get_ip_string() + ":" + get_port_string();
}

std::string udpf_interface::endpoint_addr::get_ip_string()
{
	char ip_str_buff[24];
	inet_ntop(AF_INET, &ep_addr.ipv4.netip, ip_str_buff, 24);
	return std::string(ip_str_buff);
}

std::string udpf_interface::endpoint_addr::get_port_string()
{
	return std::to_string(get_port());
}

udpf_interface::u16 udpf_interface::endpoint_addr::get_port()
{
	return ntohs(ep_addr.netport);
}

udpf_interface::u16 udpf_interface::endpoint_addr::get_net_port()
{
	return ep_addr.netport;
}

udpf_interface::u32 udpf_interface::endpoint_addr::get_net_ip()
{
	return ep_addr.ipv4.netip;
}

udpf_interface::endpoint_addr::endpoint_addr(endpoint_addr_in in_addr):
	ep_addr{ in_addr }
{
}

std::optional<udpf_interface::endpoint_connection_request> udpf_interface::listener::listen() {
	udpf_impl::listener& lref{ udpf_impl::get_endpoint(local_addr.ep_addr).lr };
	return lref.listen();
}

udpf_interface::connection udpf_interface::listener::accept(endpoint_connection_request ecreq)
{
	udpf_impl::listener& lref{ udpf_impl::get_endpoint(local_addr.ep_addr).lr };
	endpoint_connection_request nego_req{ecreq};
	nego_req.second.max_datagram_size = (ecreq.second.max_datagram_size < con_config.max_datagram_size) ? ecreq.second.max_datagram_size : con_config.max_datagram_size;
	nego_req.second.max_window_size = (ecreq.second.max_window_size < con_config.max_window_size) ? ecreq.second.max_window_size : con_config.max_window_size;
	nego_req.second.packet_timeout_mili = (ecreq.second.packet_timeout_mili < con_config.packet_timeout_mili) ? ecreq.second.packet_timeout_mili : con_config.packet_timeout_mili;
	nego_req.second.connection_timeout_mili = (ecreq.second.connection_timeout_mili < con_config.connection_timeout_mili) ? ecreq.second.connection_timeout_mili : con_config.connection_timeout_mili;

	udpf_impl::connection& conn = lref.accept(nego_req);

	return connection(conn.local, conn.remote);
}

void udpf_interface::listener::close() {
	udpf_impl::get_endpoint(local_addr.ep_addr).lr.close();
}

udpf_interface::listener::listener(endpoint_addr_in epaddrin)
	: local_addr{epaddrin}
{
	udpf_impl::endpoint_config epcfg;
	open_endpoint(epaddrin, epcfg);
}

std::optional<udpf_interface::connection> udpf_interface::connect(endpoint_addr_in& local, endpoint_addr_in& peer, connection_config& concfg)
{
	endpoint_config epcfg;
	auto conn = udpf_impl::connect(local, peer, epcfg, concfg);
	if (!conn)
		return std::nullopt;
	return connection(conn.value()->local, conn.value()->remote);
}

udpf_interface::endpoint_addr_in udpf_interface::endpoint_addr_info(u16 port, u32 netip)
{
	endpoint_addr_in out;
	if (netip) {
		out.ipv4.netip = netip;
		out.netport = htons(port);
	}
	else {
		char host[64];
		gethostname(host, 64);
		addrinfo hints{};
		SecureZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;			// IPv4
		// For UDP use SOCK_DGRAM instead of SOCK_STREAM.
		hints.ai_socktype = SOCK_DGRAM;	// Reliable delivery
		// Could be 0 for autodetect, but reliable delivery over IPv4 is always TCP.
		hints.ai_protocol = IPPROTO_UDP;	// TCP
		// Create a passive socket that is suitable for bind() and listen().
		hints.ai_flags = AI_PASSIVE;
		addrinfo* info = nullptr;
		std::string portString{ std::to_string(port) };
		getaddrinfo(host, portString.c_str(), &hints, &info);
		memcpy(&out.ipv4.netip, info->ai_addr->sa_data + 2, sizeof(u32));
	}
	return out;
}
