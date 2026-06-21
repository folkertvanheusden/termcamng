#include <atomic>
#include <thread>


class terminal;

class VNCServer
{
private:
	terminal        *const t   { nullptr };
	const int        port      { 5901    };
	const bool       vnc_allow_keyboard { false };
	const int        stdin_fd  { -1      };
	std::atomic_bool stop_flag { false   };
	std::thread     *th        { nullptr };

	struct client_state {
		bool ctrl_pressed;
	};

	bool VNCSendVersion      (int fd);
	bool VNCSecurityHandshake(int fd);
	bool VNCClientServerInit (int fd);
	bool VNCWaitForEvent     (int fd, client_state *const cs);
	bool VNCSendFrame        (int fd, bool first);
	void VNCClientThread     (int fd);

public:
	VNCServer(terminal *const t, const int port, const bool vnc_allow_keyboard, const int stdin_fd) :
		t(t), port(port), vnc_allow_keyboard(vnc_allow_keyboard), stdin_fd(stdin_fd) {
	}

	~VNCServer() {
		stop_flag = true;
		if (th) {
			th->join();
			delete th;
		}
	}

	void begin() {
		th = new std::thread(std::ref(*this));
	}

	void operator()();
};
