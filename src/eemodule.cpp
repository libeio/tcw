
#include "eemodule.h"
#include "eehandler.h"
#include "eehelper.h"
#include "eelog.h"

ssize_t daemon_read_callback(int fd, void *buf, size_t size, void *userp)
{
    (void) buf;
    (void) size;
        
    tcw::EventHandler *eeh = (tcw::EventHandler *)userp;
    
    tcw::BaseClient *bc = dynamic_cast<tcw::BaseClient*>(eeh->m_clients[fd]);
    if (! bc) {
        return -1;
    }
    
    bool from_outward = false;
    if (eeh->m_olinkers.find(fd) != eeh->m_olinkers.end()) {
        from_outward = true;
    } else {
        from_outward = false;
    }
    
    char hbuf[NEGOHSIZE];
    ssize_t nh = read(fd, hbuf, NEGOHSIZE);
    if (nh != NEGOHSIZE) {
        Erro(eeh->logger, MODU, "read(%ld): %s", nh, strerror(errno));
        return -1;
    }

    NegoHeader header;
    memcpy(&header, hbuf, NEGOHSIZE);

    uint64_t origin = header.origin;
    uint64_t orient = header.orient;
    size_t bodysize = ntohs(header.bodysize);
    if (bodysize == 0 && orient == eeh->m_id) {
        if (header.ver[0] == (uint8_t)'h' && header.ver[1] == (uint8_t)'b') {
            Info(eeh->logger, MODU, "receive heartbeat from %s(sid=%lu)", eeh->m_services_id[origin].c_str(), origin);
            eeh->m_heartbeats[bc->sid] = now_time();
        }
        return 0;
    }

    uint16_t msgid = ntohs(header.msgid);

    char *rbuf = (char *)calloc(1, bodysize);
    if (! rbuf) {
        return -1;
    }

    ssize_t nb = read(fd, rbuf, bodysize);
    if (nb != (ssize_t)bodysize) {
        Erro(eeh->logger, MODU, "read(%ld != %lu): %s", nb, bodysize, strerror(errno));
        if (rbuf) {
            free(rbuf);
        }
        return -1;
    }

    std::string msg(rbuf, nb);
    if (rbuf) {
        free(rbuf);
    }
    
#ifdef DEBUG
    Dbug(eeh->logger, MODU, "msgid=%u, origin=%s, orient=%s, from_outward=%d",
                                msgid, eeh->m_services_id[origin].c_str(), 
                                eeh->m_services_id[orient].c_str(), from_outward);
#endif

    decltype (std::declval<std::map<tcw::FD_t, tcw::SID_t>>().begin()) iterTo;
    int tofd = -1;
    if (from_outward) {
        /** socket connect: recv */
        iterTo = std::find_if(eeh->m_ilinkers.begin(), eeh->m_ilinkers.end(),
                                [&orient](decltype(*eeh->m_ilinkers.begin())& ele){
            return ele.second == orient;
        });
        if (iterTo != eeh->m_ilinkers.end()) {
            tofd = iterTo->first;
            eeh->m_route_fd[origin].insert(fd);
        }
    } else {   
        iterTo = std::find_if(eeh->m_ilinkers.begin(), eeh->m_ilinkers.end(),
                                [&orient](decltype(*eeh->m_ilinkers.begin())& ele){
            return ele.second == orient;
        });
        /** ipc between internal child process */
        if (iterTo != eeh->m_ilinkers.end()) {
            tofd = iterTo->first;
        } else {
            /** socket connect: send  load balance */
            /** load balance */
            if (! eeh->m_route_fd[orient].empty()) {
                size_t idx = rand() % eeh->m_route_fd[orient].size();
                auto itProxy = eeh->m_route_fd[orient].begin();
                for ( ; itProxy != eeh->m_route_fd[orient].end(); itProxy++) {
                    if (idx-- == 0) {
                        break;
                    }
                }
                if (itProxy != eeh->m_route_fd[orient].end()) {
                    tofd = *itProxy;
                }
            } else if (! eeh->m_olinkers.empty()) {
                size_t idx = rand() % eeh->m_olinkers.size();
                for (iterTo = eeh->m_olinkers.begin(); iterTo != eeh->m_olinkers.end(); iterTo++) {
                    if (idx-- == 0) {
                        break;
                    }
                }
                if (iterTo != eeh->m_olinkers.end()) {
                    tofd = iterTo->first;
                    eeh->m_route_fd[orient].insert(tofd);
                }
            }
            if (tofd > 0) {
                std::string smsg = std::string(hbuf, hbuf + sizeof(hbuf)) + msg;
                size_t nt = write(tofd, smsg.c_str(), smsg.size());
                if (nt != smsg.size()) {
                    Erro(eeh->logger, MODU, "write: %s", strerror(errno));
                    return -1;
                }
                
                return 0;
            }
        }
    }

    if (tofd <= 0) {
        /**
         * maybe enter here if peer is closed suddenly while transferring.
         * we tolerate this exception.
         */
        Erro(eeh->logger, MODU, "could not find fd to write");
        return -1;
    }

    tcw::BaseClient *tobc = dynamic_cast<tcw::BaseClient*>(eeh->m_clients[tofd]);
    if (! tobc) {
        return -1;
    }

    /** recover it to the original message(header + msg) */
    if (! eeh->m_linker_queues[tobc->sid].try_enqueue(std::string(hbuf, hbuf + sizeof(hbuf)) + msg)) {
        Erro(eeh->logger, MODU, "enqueue failed");
        return -1;
    }

#ifdef DEBUG
    Dbug(eeh->logger, MODU, "pushed msg(len=%lu, from=%s) to que(ownby=%s, size=%lu) and forward to %s", 
                                msg.size(), eeh->m_services_id[origin].c_str(),
                                eeh->m_services_id[tobc->sid].c_str(), eeh->m_linker_queues[tobc->sid].size_approx(),
                                eeh->m_services_id[orient].c_str());
#endif

    eeh->tcw_mod(tobc, EPOLLOUT | EPOLLHUP | EPOLLRDHUP);

    return 0;
}

/** do nothing, write purely */
ssize_t daemon_write_callback(int fd, const void *buf, size_t count, void *userp)
{
    (void) buf;
    (void) count;

    tcw::EventHandler *eeh = (tcw::EventHandler *)userp;
    
    tcw::BaseClient *bc = dynamic_cast<tcw::BaseClient*>(eeh->m_clients[fd]);
    if (! bc) {
        return -1;
    }
    
    tcw::SID_t sid;
    if (eeh->m_ilinkers.find(fd) != eeh->m_ilinkers.end()) {
        sid = eeh->m_ilinkers[fd];
    } else if (eeh->m_olinkers.find(fd) != eeh->m_olinkers.end()) {
        sid = eeh->m_olinkers[fd];
    } else {
        Erro(eeh->logger, MODU, "an exceptions occurs");
        return -1;
    }

#ifdef DEBUG
    Dbug(eeh->logger, MODU, "do write to ec(%p, t=%d, s=%s, queue_size=%lu)", 
                    bc, bc->type, eeh->m_services_id[sid].c_str(), eeh->m_linker_queues[sid].size_approx());
#endif

    while (eeh->m_linker_queues[sid].size_approx() > 0) {
        std::string msg;
        eeh->m_linker_queues[sid].try_dequeue(msg);
        size_t nt = write(fd, msg.c_str(), msg.size());
        if (nt != msg.size()) {
            Erro(eeh->logger, MODU, "write: %s", strerror(errno));
            return -1;
        }
#ifdef DEBUG
        Dbug(eeh->logger, MODU, "forwarded msg(len=%lu) to peer end of ec(%p, t=%d)", nt, bc, bc->type);
#endif
    }
    
    return 0;
}

int daemon_timer_callback(void *args, void *userp)
{   
    (void) args;
    (void) userp;
    
    return 0;
}

event_actions_t daemon_callback_module = {
    daemon_read_callback,
    daemon_write_callback,
    daemon_timer_callback,
};

/** do nothing, read purely */
ssize_t child_read_callback(int fd, void *buf, size_t size, void *userp)
{
    (void) buf;
    (void) size;
    
    tcw::EventHandler *eeh = (tcw::EventHandler *)userp;
    
    tcw::BaseClient *bc = dynamic_cast<tcw::BaseClient*>(eeh->m_clients[fd]);
    if (! bc) {
        return -1;
    }
    
#ifdef DEBUG
    Dbug(eeh->logger, MODU, "do read from ec(%p, t=%d, s=%s)", bc, bc->type, eeh->m_services_id[bc->sid].c_str());
#endif

    char hbuf[NEGOHSIZE];
    ssize_t nh = read(fd, hbuf, NEGOHSIZE);
    if (nh != NEGOHSIZE) {
        Erro(eeh->logger, MODU, "read(%ld != %lu): %s", nh, NEGOHSIZE, strerror(errno));
        return -1;
    }
    
    NegoHeader header;
    memcpy(&header, hbuf, NEGOHSIZE);
    
    size_t bodysize = ntohs(header.bodysize);
    
    char *rbuf = (char *)calloc(1, bodysize);
    if (! rbuf) {
        return -1;
    }
    
    ssize_t nb = read(fd, rbuf, bodysize);
    if (nb != (ssize_t)bodysize) {
        Erro(eeh->logger, MODU, "read(%ld != %lu): %s", nb, bodysize, strerror(errno));
        if (rbuf) {
            free(rbuf);
        }
        return -1;
    }
    
    std::string msg(rbuf, nb);
    if (rbuf) {
        free(rbuf);
    }
    
#ifdef DEBUG
    Dbug(eeh->logger, MODU, "received msg(len=%lu)", msg.size());
#endif

    /** received it and do nothing, instead of passing it to application layer. */
    eeh->m_messages.try_enqueue(std::string(hbuf, hbuf + sizeof(hbuf)) + std::move(msg));

#ifdef DEBUG
    Dbug(eeh->logger, MODU, "notified to deal with msg queue(size=%lu)", eeh->m_messages.size_approx());
#endif

    return 0; 
}

/** do nothing, write purely */
ssize_t child_write_callback(int fd, const void *buf, size_t count, void *userp)
{
    (void) buf;
    (void) count;
    
    tcw::EventHandler *eeh = (tcw::EventHandler *)userp;
        
    tcw::BaseClient *bc = dynamic_cast<tcw::BaseClient*>(eeh->m_clients[fd]);
    if (! bc) {
        return -1;
    }
    
    tcw::SID_t sid;
    if (eeh->m_ilinkers.find(fd) != eeh->m_ilinkers.end()) {
        sid = eeh->m_ilinkers[fd];
    } else {
        Erro(eeh->logger, MODU, "an exceptions occurs");
        return -1;
    }
    
#ifdef DEBUG
    Dbug(eeh->logger, MODU, "do write to ec(%p, t=%d, s=%s, queue_size=%lu)", 
                                bc, bc->type,
                                eeh->m_services_id[sid].c_str(), eeh->m_linker_queues[sid].size_approx());
#endif

    while (eeh->m_linker_queues[sid].size_approx() > 0) {        
        std::string msg;
        eeh->m_linker_queues[sid].try_dequeue(msg);
        size_t nt = write(fd, msg.c_str(), msg.size());
        if (nt != msg.size()) {
            Erro(eeh->logger, MODU, "write: %s", strerror(errno));
            return -1;
        }

#ifdef DEBUG
        Dbug(eeh->logger, MODU, "forwarded msg(len=%lu) to peer end of ec(%p, t=%d)", nt, bc, bc->type);
#endif
    }
    
    return 0;
}

int child_timer_callback(void *args, void *userp)
{
    tcw::EventHandler *eeh = (tcw::EventHandler *)userp;
    tcw::BaseClient *bc = dynamic_cast<tcw::BaseClient*>((tcw::EClient*)args);
    if (! bc) {
        return -1;
    }

    if (eeh->m_ilinkers.find(bc->fd) != eeh->m_ilinkers.end()) {
        if (now_time() - bc->heartbeat < HEART_BEAT_INTERVAL * 1000) {
            return 0;
        }
        bc->heartbeat = now_time();

        NegoHeader header;
        header.ver[0] = (uint8_t)'h';
        header.ver[1] = (uint8_t)'b';
        header.bodysize = htons(0);
        header.origin = eeh->m_id;
        header.orient = eeh->m_daemon_id;

        std::string tostream(std::string((const char *)&header, NEGOHSIZE));

        if(! eeh->m_linker_queues[bc->sid].try_enqueue(tostream)) {
            Erro(eeh->logger, MODU, "enqueue failed");
            return -1;
        }

#ifdef DEBUG
        Dbug(eeh->logger, MODU, "pushed msg(len=%lu, from=%s) to que(ownby=%s, size=%lu) and heartbeat to %s", 
                                    tostream.size(), eeh->m_services_id[eeh->m_id].c_str(),
                                    eeh->m_services_id[bc->sid].c_str(), eeh->m_linker_queues[bc->sid].size_approx(),
                                    eeh->m_services_id[eeh->m_daemon_id].c_str());
#endif
        eeh->tcw_mod(bc, EPOLLOUT | EPOLLHUP | EPOLLRDHUP);
    }

    return 0;
}

event_actions_t child_callback_module = {
    child_read_callback,
    child_write_callback,
    child_timer_callback,
};
