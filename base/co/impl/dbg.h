#include "../../log.h"
#include "../co.h"

#ifdef CODBG
#define SOLOG DLOG << "S" << co::sched_id() << "| "
#define COLOG DLOG << "S" << co::sched_id() << "/C" << co::coroutine_id() << "| "
#define COTID(it) *(uint64*)(&(it))
#else
#define SOLOG LOG_IF(false)
#define COLOG LOG_IF(false)
#define COTID(it) 0
#endif
