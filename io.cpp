#include <stdint.h>
#include <unistd.h>


bool WRITE(int fd, const uint8_t *from, size_t len)
{
        while(len > 0) {
                ssize_t rc = write(fd, from, len);
                if (rc <= 0)
                        return false;

		from += rc;
		len  -= rc;
        }

        return true;
}

bool READ(int fd, uint8_t *whereto, size_t len)
{
        while(len > 0) {
                ssize_t rc = read(fd, whereto, len);
                if (rc <= 0)
                        return false;

		whereto += rc;
		len     -= rc;
        }

        return true;
}
