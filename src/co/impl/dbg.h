#pragma once

#ifdef CODBG
#define SOLOG LOG << 'S' << co::sched_id() << ' '
#define COLOG LOG << 'S' << co::sched_id() << '.' << co::coroutine_id() << ' '
#else
#define SOLOG LOG_IF(false)
#define COLOG LOG_IF(false)
#endif
