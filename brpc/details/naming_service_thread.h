// Baidu RPC - A framework to host and access services throughout Baidu.
// Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved

// Author: The baidu-rpc authors (pbrpc@baidu.com)
// Date: Sun Aug 31 20:02:37 CST 2014

#ifndef BRPC_NAMING_SERVICE_THREAD_H
#define BRPC_NAMING_SERVICE_THREAD_H

#include <string>
#include "base/intrusive_ptr.hpp"               // base::intrusive_ptr
#include "bthread/bthread.h"                    // bthread_t
#include "brpc/server_id.h"                     // ServerId
#include "brpc/shared_object.h"                 // SharedObject
#include "brpc/naming_service.h"                // NamingService
#include "brpc/naming_service_filter.h"         // NamingServiceFilter


namespace brpc {

// Inherit this class to observer NamingService changes.
// NOTE: Same SocketId with different tags are treated as different entries.
// When you change tag of a server, the server with the old tag will appear
// in OnRemovedServers first, then in OnAddedServers with the new tag.
class NamingServiceWatcher {
public:
    virtual ~NamingServiceWatcher() {}
    virtual void OnAddedServers(const std::vector<ServerId>& servers) = 0;
    virtual void OnRemovedServers(const std::vector<ServerId>& servers) = 0;
};

struct GetNamingServiceThreadOptions {
    GetNamingServiceThreadOptions()
        : succeed_without_server(false)
        , log_succeed_without_server(true) {}
    
    bool succeed_without_server;
    bool log_succeed_without_server;
};

// A dedicated thread to map a name to ServerIds
class NamingServiceThread : public SharedObject, public Describable {
    struct ServerNodeWithId {
        ServerNode node;
        SocketId id;

        inline bool operator<(const ServerNodeWithId& rhs) const {
            return id != rhs.id ? (id < rhs.id) : (node < rhs.node);
        }
    };
    class Actions : public NamingServiceActions {
    public:
        Actions(NamingServiceThread* owner);
        ~Actions();
        void AddServers(const std::vector<ServerNode>& servers);
        void RemoveServers(const std::vector<ServerNode>& servers);
        void ResetServers(const std::vector<ServerNode>& servers);
        int WaitForFirstBatchOfServers();
        void EndWait(int error_code);

    private:
        NamingServiceThread* _owner;
        bthread_id_t _wait_id;
        base::atomic<bool> _has_wait_error;
        int _wait_error;
        std::vector<ServerNode> _last_servers;
        std::vector<ServerNode> _servers;
        std::vector<ServerNode> _added;
        std::vector<ServerNode> _removed;
        std::vector<ServerNodeWithId> _sockets;
        std::vector<ServerNodeWithId> _added_sockets;
        std::vector<ServerNodeWithId> _removed_sockets;
    };

public:    
    NamingServiceThread();
    ~NamingServiceThread();

    int Start(const NamingService* ns, const std::string& service_name,
              const GetNamingServiceThreadOptions* options);
    int WaitForFirstBatchOfServers();

    int AddWatcher(NamingServiceWatcher* w, const NamingServiceFilter* f);
    int AddWatcher(NamingServiceWatcher* w) { return AddWatcher(w, NULL); }
    int RemoveWatcher(NamingServiceWatcher* w);

    void Describe(std::ostream& os, const DescribeOptions&) const;

private:
    void Run();
    static void* RunThis(void*);

    static void ServerNodeWithId2ServerId(
        const std::vector<ServerNodeWithId>& src,
        std::vector<ServerId>* dst, const NamingServiceFilter* filter);

    base::Mutex _mutex;
    bthread_t _tid;
    // TODO: better use a name.
    const NamingService* _source_ns;
    NamingService* _ns;
    std::string _service_name;
    GetNamingServiceThreadOptions _options;
    std::vector<ServerNodeWithId> _last_sockets;
    Actions _actions;
    std::map<NamingServiceWatcher*, const NamingServiceFilter*> _watchers;
};

std::ostream& operator<<(std::ostream& os, const NamingServiceThread&);

// Get the decicated thread associated with `url' and put the thread into
// `ns_thread'. Calling with same `url' shares and returns the same thread.
// If the url is not accessed before, this function blocks until the
// NamingService returns the first batch of servers. If no servers are
// available, unless `options->succeed_without_server' is on, this function
// returns -1.
// Returns 0 on success, -1 otherwise.
int GetNamingServiceThread(base::intrusive_ptr<NamingServiceThread>* ns_thread,
                           const char* url,
                           const GetNamingServiceThreadOptions* options);

} // namespace brpc


#endif  // BRPC_NAMING_SERVICE_THREAD_H