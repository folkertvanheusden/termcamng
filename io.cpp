#include <stdint.h>
#include <unistd.h>


ssize_t WRITE(int fd, const uint8_t *whereto, size_t len)
{
        ssize_t cnt = 0;

        while(len > 0) {
                ssize_t rc = write(fd, whereto, len);

                if (rc <= -1)
                        return rc;

		whereto += rc;
		len -= rc;
		cnt += rc;
        }

        return cnt;
}

