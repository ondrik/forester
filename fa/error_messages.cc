#include "error_messages.hh"

const std::string ErrorMessages::DEREFERENCED = "dereferenced value is not a valid reference";
const std::string ErrorMessages::ALLOCATE_MISMATCH = "allocated block size mismatch";
const std::string ErrorMessages::INVALID_RELEASE = "releasing a pointer which points inside an allocated block";
const std::string ErrorMessages::GARBAGE_DETECTED = "garbage detected";
