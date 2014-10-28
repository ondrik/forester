#ifndef ERROR_MESSAGES
#define ERROR_MESSAGES

#include <string>

class ErrorMessages
{
public: // public messages
	static const std::string DEREFERENCED;
	static const std::string ALLOCATE_MISMATCH;
	static const std::string INVALID_RELEASE;
	static const std::string GARBAGE_DETECTED;
	static const std::string GARBAGE_DETECTED_EXIT;
	static const std::string NORET;
};

#endif
