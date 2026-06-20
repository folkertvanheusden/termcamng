#include <atomic>
#include <thread>


class terminal;

class VNCServer
{
private:
	terminal        *const t   { nullptr };
	const int        port      { 5901    };
	std::atomic_bool stop_flag { false   };
	std::thread     *th        { nullptr };

	void VNCSendVersion(int fd);
	void VNCSecurityHandshake(int fd);
	void VNCClientServerInit(int fd);
	bool VNCWaitForEvent(int fd);
	bool VNCSendFrame(int fd, bool first);
	void VNCClientThread(int fd);

public:
	VNCServer(terminal *const t, const int port) : t(t), port(port) {
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
