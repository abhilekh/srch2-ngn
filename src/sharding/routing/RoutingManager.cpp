#include "RoutingManager.h"
#include "server/MongodbAdapter.h"

using namespace srch2::httpwrapper;

namespace srch2 {
namespace httpwrapper {


RoutingManager::RoutingManager(ConfigManager&  cm, TransportManager& transportManager)  : 
    						configurationManager(cm),  transportManager(transportManager), dpInternal(&cm),
    						internalMessageBroker(*this, dpInternal),
    						shardServers(new Srch2Server[cm.getCoreInfoMap().size()]) {


	// TODO : do we have one Srch2Server per core?
	// create a server (core) for each data source in config file
    unsigned coreInfoIndex = 0 ;
	for(ConfigManager::CoreInfoMap_t::const_iterator iterator =
			cm.coreInfoIterateBegin(); iterator != cm.coreInfoIterateEnd();
			iterator++) {
		Srch2Server *core = &shardServers[coreInfoIndex];
		core->setCoreName(iterator->second->getName());

		if(iterator->second->getDataSourceType() ==
				srch2::httpwrapper::DATA_SOURCE_MONGO_DB) {
			// set current time as cut off time for further updates
			// this is a temporary solution. TODO
			MongoDataSource::bulkLoadEndTime = time(NULL);
			//require srch2Server
			MongoDataSource::spawnUpdateListener(core);
		}

		//load the index from the data source
		try{
			core->init(&cm);
		} catch(exception& ex) {
			/*
			 *  We got some fatal error during server initialization. Print the error
			 *  message and exit the process.
			 *
			 *  Note: Other internal modules should make sure that no recoverable
			 *        exception reaches this point. All exceptions that reach here are
			 *        considered fatal and the server will stop.
			 */
			Logger::error(ex.what());
			exit(-1);
		}
		//
		coreInfoIndex ++;
	}
}

ConfigManager* RoutingManager::getConfigurationManager() {
	return &this->configurationManager;

}

DPInternalRequestHandler* RoutingManager::getDpInternal() {
	return &this->dpInternal;
}

InternalMessageBroker * RoutingManager::getInternalMessageBroker(){
	return &this->internalMessageBroker;
}

MessageAllocator * RoutingManager::getMessageAllocator() {
	return transportManager.getMessageAllocator();
}

} }
//save indexes in deconstructor
