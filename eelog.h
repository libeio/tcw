
#ifndef __Epoll_Event_LOG_H__
#define __Epoll_Event_LOG_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace EEHNS {

/** index of fundmental log type */
#define LOG_TYPE_GLOB               0
#define LOG_TYPE_HAND               1
#define LOG_TYPE_POLI               2
#define LOG_TYPE_TRAN               3
#define LOG_TYPE_ROVE               4
#define LOG_TYPE_SYNC               5
#define LOG_TYPE_RESO               6
#define LOG_TYPE_MADO               7
#define LOG_TYPE_CHRO               8
#define LOG_TYPE_GIMM               9
#define LOG_TYPE_SUBS              10
/** index of user's application log type */
#define LOG_TYPE_USER_MADOLCHE         21
#define LOG_TYPE_USER_CHRONOMALY       22
#define LOG_TYPE_USER_GIMMICK_PUPPET   23
/** the first index of extend log type */
#define LOG_TYPE_FIRST_EXT_ID 32

/** log level */
#define LOG_LEVEL_EMER     1U
#define LOG_LEVEL_ALER     2U
#define LOG_LEVEL_CRIT     3U
#define LOG_LEVEL_ERRO     4U
#define LOG_LEVEL_WARN     5U
#define LOG_LEVEL_NOTI     6U
#define LOG_LEVEL_INFO     7U
#define LOG_LEVEL_DBUG     8U

struct log_type {
    const char *log_type_name;
    uint32_t    log_type_level;
};

struct log_type_reg_table {
    uint32_t    log_type_id;
    const char *log_type_name;
};

class Logger
{
public:
    Logger(const char* dir, 
           const char* name, 
           uint32_t limit_size = 1, 
           uint32_t level = LOG_LEVEL_DBUG);
    ~Logger();
public:
    void      log_init(void);
    void      log_free(void);
    int       log_open_stream(FILE* f);
    void      log_set_global_level(uint32_t level);
    uint32_t  log_get_global_level(void);
    int       log_get_level(uint32_t logtype);
    int       log_set_level(uint32_t logtype, uint32_t level);
    int       log_register(const char *name);
    void      log_print_regtab();
    int       log_out(uint32_t level, uint32_t logtype, const char *format, ...);
private:
    int       log_lookup(const char *name);
    int       log_vlog(uint32_t level, uint32_t logtype, const char *format, va_list ap);
    int       log_register_internal(const char *name, int id);

private:
    uint32_t         m_nTypeMask;
    uint32_t         m_nLevel;
    const char*      m_szPath;
    uint32_t         m_nLimitSize;
    FILE*            m_pFile;
    struct log_type* m_pLogTypes;
    size_t           m_nLogTypesSize;
};

static const struct log_type_reg_table reg_table[] = {
    { LOG_TYPE_GLOB,     "global" },
    { LOG_TYPE_HAND,     "handler" },
    { LOG_TYPE_POLI,     "policy module" },
    { LOG_TYPE_TRAN,     "transfer module" },
    { LOG_TYPE_ROVE,     "rover module" },
    { LOG_TYPE_SYNC,     "synchron module" },
    { LOG_TYPE_RESO,     "resonator module" },
    { LOG_TYPE_MADO,     "madolche module" },
    { LOG_TYPE_CHRO,     "chronomaly module" },
    { LOG_TYPE_GIMM,     "gimmick puppet module" },
    { LOG_TYPE_SUBS,     "sub service module" },
    
    { LOG_TYPE_USER_MADOLCHE,           "madolche user application" },
    { LOG_TYPE_USER_CHRONOMALY,         "chronomaly user application" },
    { LOG_TYPE_USER_GIMMICK_PUPPET,     "gimmick puppet user application" }
};

};

#endif // ! __Epoll_Event_LOG_H__