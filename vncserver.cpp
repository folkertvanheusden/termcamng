#include <cstdio>
#include <cstring>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include "io.h"
#include "logging.h"
#include "net.h"
#include "terminal.h"
#include "utils.h"
#include "vncserver.h"


void VNCServer::VNCSendVersion(int fd)
{
	const char msg[] = "RFB 003.008\n";
	WRITE(fd, reinterpret_cast<const uint8_t *>(msg), strlen(msg));

	// wait for reply, ignoring what it is
	for(;;) {
		char buffer = 0;
		if (READ(fd, reinterpret_cast<uint8_t *>(&buffer), 1) != 1)
			break;
		if (buffer == '\n')
			break;
	}
	dolog(ll_debug, "VNC: version received");
}

void VNCServer::VNCSecurityHandshake(int fd)
{
	uint8_t list[] { 1, 1 };  // 1, None
	WRITE(fd, list, sizeof list);

	// receive reply with choice, ignoring choice
	char buffer = 0;
	READ(fd, reinterpret_cast<uint8_t *>(&buffer), 1);

	uint8_t reply[4] { };
	WRITE(fd, reply, sizeof reply);

	dolog(ll_debug, "VNC: end of SecurityHandshake");
}

void VNCServer::VNCClientServerInit(int fd)
{
	uint8_t shared = 0;
	READ(fd, &shared, 1);

	int      width  = 0;
	int      height = 0;
	uint8_t *dummy = nullptr;
	t->render(&dummy, &width, &height);
	delete [] dummy;

	uint8_t reply[24] { };
	reply[0] = width >> 8;
	reply[1] = width & 255;
	reply[2] = height >> 8;
	reply[3] = height & 255;
	reply[4] = 32;  // bits per pixel
	reply[5] = 32;  // depth
	reply[6] = 1;  // big endian
	reply[7] = 1;  // True color
	reply[8] = 0;  // red max
	reply[9] = 255;  // red max
	reply[10] = 0;  // green max
	reply[11] = 255;  // green max
	reply[12] = 0;  // blue max
	reply[13] = 255;  // blue max
	reply[14] = 16;  // red shift
	reply[15] = 8;  // green shift
	reply[16] = 0;  // blue shift
	reply[17] = reply[18] = reply[19] = 0;  // padding
	const char name[] = "termcamng";
	size_t name_len = strlen(name);
	reply[20] = (name_len >> 24) & 255;
	reply[21] = (name_len >> 16) & 255;
	reply[22] = (name_len >>  8) & 255;
	reply[23] = name_len & 255;
	WRITE(fd, reply, 24);
	WRITE(fd, reinterpret_cast<const uint8_t *>(name), name_len);

	dolog(ll_debug, "VNC: end of ClientServerInit");
}

bool VNCServer::VNCWaitForEvent(int fd)
{
	int wait = 1000 / 10;
	pollfd fds[1] { { fd, POLLIN, 0 } };

	for(;;) {
		int rc = poll(fds, 1, wait);
		if (rc == 0)
			return true;
		if (rc == -1)
			return false;

		wait = 0;

		uint8_t type_ = 0;
		READ(fd, &type_, 1);

		if (type_ == 0) {  // SetPixelFormat
			uint8_t buffer[3 + 16];
			READ(fd, buffer, sizeof buffer);
		}
		else if (type_ == 2) {  // SetEncodings
			uint8_t buffer[3];
			READ(fd, buffer, sizeof buffer);

			int no_encodings = (buffer[1] << 8) | buffer[2];
			for(int i=0; i<no_encodings; i++) {
				uint8_t temp[4];
				READ(fd, temp, sizeof temp);
			}
		}
		else if (type_ == 3) {  // FramebufferUpdateRequest
			uint8_t buffer[9];
			READ(fd, buffer, sizeof buffer);
			// TODO
		}
		else if (type_ == 4) {  // KeyEvent
			uint8_t buffer[7];
			if (READ(fd, buffer, sizeof buffer) != sizeof buffer)
				break;
			bool down = buffer[0];
			uint32_t vnc_scan_code = (buffer[3] << 24) | (buffer[4] << 16) | (buffer[5] << 8) | buffer[6];
			dolog(ll_debug, "VNC: key pressed with scan code %u (%d)", vnc_scan_code, down);
			if (down) {
				if (vnc_scan_code < 0x80) {
					uint8_t buffer = vnc_scan_code;
					WRITE(stdin_fd, &buffer, 1);
				}
				else if (vnc_scan_code == 65293) {
					uint8_t buffer[] = { 13, 10 };
					WRITE(stdin_fd, buffer, sizeof buffer);
				}
			}
		}
		else if (type_ == 5) {  // PointerEvent
			uint8_t buffer[5];
			READ(fd, buffer, sizeof buffer);
		}
		else if (type_ == 6) {  // ClientCutText
			uint8_t buffer[7];
			READ(fd, buffer, sizeof buffer);
			uint32_t n_to_read = (buffer[3] << 24) | (buffer[4] << 16) | (buffer[5] << 8) | buffer[6];
			for(uint32_t i=0; i<n_to_read; i++) {
				uint8_t buffer[1];
				READ(fd, buffer, sizeof buffer);
			}
		}
		else {
			dolog(ll_info, "VNC: Client message %d not understood", type_);
			return false;
		}
	}

	return true;
}

bool VNCServer::VNCSendFrame(int fd, bool first)
{
	int      w      = 0;
	int      h      = 0;
	uint8_t *pixels = nullptr;
	t->render(&pixels, &w, &h);

	uint8_t update[4 + 12];
	update[0] = 0;  // FrameBufferUpdate
	update[1] = 0;  // padding
	update[2] = 0;  // 1 rectangle
	update[3] = 1;
	update[4] = 0;  // x pos
	update[5] = 0;  // x pos
	update[6] = 0;  // y pos
	update[7] = 0;  // y pos
	update[8] = w >> 8;  // width
	update[9] = w;
	update[10] = h >> 8;  // height
	update[11] = h;
	update[12] = 0;
	update[13] = 0;
	update[14] = 0;
	update[15] = 0;

	if (WRITE(fd, update, sizeof update) != sizeof update) {
		dolog(ll_info, "VNC: failed transmitting header");
		delete [] pixels;
		return false;
	}

	uint8_t *temp = new uint8_t[w * h * 4]();
	for(int i=0; i<w*h; i++) {
		int out_off = i * 4;
		int in_off  = i * 3;
		temp[out_off + 2] = pixels[in_off + 0];
		temp[out_off + 1] = pixels[in_off + 1];
		temp[out_off + 0] = pixels[in_off + 2];
	}
	delete [] pixels;

	if (WRITE(fd, temp, w * h * 4) != w * h * 4) {
		dolog(ll_info, "VNC: failed transmitting payload");
		delete [] temp;
		return false;
	}

	delete [] temp;

	return true;
}

void VNCServer::VNCClientThread(int fd)
{
	VNCSendVersion(fd);
	VNCSecurityHandshake(fd);
	VNCClientServerInit(fd);

	uint64_t ts_after = 0;

	bool first = true;
	while(!stop_flag) {
		if (t->wait_for_frame(&ts_after, 10)) {
			if (VNCSendFrame(fd, first) == false)
				break;
			first = false;
		}

		if (VNCWaitForEvent(fd) == false)
			break;
	}

	dolog(ll_info, "VNC: session via fd %d terminated", fd); 
}

void VNCServer::operator()()
{
	set_thread_name("vnc-server");

	signal(SIGPIPE, SIG_IGN);

	int s = start_tcp_listen("0.0.0.0", port);
	dolog(ll_info, "VNC: server started");

	pollfd fds[] { { s, POLLIN, 0 } };

	while(!stop_flag) {
		int rc = poll(fds, 1, 100);
		if (rc == 0)
			continue;
		if (rc == -1)
			break;
		int c = accept(s, nullptr, nullptr);
		if (c == -1)
			break;

		dolog(ll_info, "VNC: incoming session accepted on fd %d", c);

		std::thread t([&] {
				VNCClientThread(c);
				close(c);
				});
		t.detach();
	}
}
