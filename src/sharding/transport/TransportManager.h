#ifndef __TRANSPORT_MANAGER_H__
#define  __TRANSPORT_MANAGER_H__

#include "RouteMap.h"
#include <event.h>
#include<pthread.h>
#include "Message.h"
#include "MessageAllocator.h"

namespace srch2 {
namespace httpwrapper {

typedef std::vector<event_base*> EventBases;
typedef std::vector<Node> Nodes;

struct TransportManager {
  struct RouteMap routeMap;
  pthread_t listeningThread;
  MessageTime_t distributedTime;
  MessageAllocator messageAllocator;

  TransportManager(EventBases&, Nodes&);
  
  MessageTime_t route(Message*);
};

}}

#endif /* __TRANSPORT_MANAGER_H__ */
