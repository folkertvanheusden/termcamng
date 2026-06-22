#include <cstdint>
#include <cstdlib>


bool READ (int fd,       uint8_t *whereto, size_t len);
bool WRITE(int fd, const uint8_t *from   , size_t len);
