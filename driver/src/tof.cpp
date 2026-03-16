#include "tof.hpp"
#include "tof320.hpp"

ToF* ToF::tof320(const char* host, const char* port)
{
	return new ToF320((char*) host, port);
}
