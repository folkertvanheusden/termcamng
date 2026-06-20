#include <stdint.h>
#include <unistd.h>


ssize_t WRITE(int fd, const uint8_t *from, size_t len)
{
        ssize_t cnt = 0;

        while(len > 0) {
                ssize_t rc = write(fd, from, len);
                if (rc <= 0)
                        return rc;

		from += rc;
		len  -= rc;
		cnt  += rc;
        }

        return cnt;
}

ssize_t READ(int fd, uint8_t *whereto, size_t len)
{
        ssize_t cnt = 0;

        while(len > 0) {
                ssize_t rc = read(fd, whereto, len);
                if (rc <= 0)
                        return rc;

		whereto += rc;
		len     -= rc;
		cnt     += rc;
        }

        return cnt;
}
