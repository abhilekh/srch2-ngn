#include "GetInfoAggregatorAndPrint.h"


namespace srch2is = srch2::instantsearch;
using namespace std;

using namespace srch2is;

namespace srch2 {
namespace httpwrapper {


GetInfoAggregatorAndPrint::GetInfoAggregatorAndPrint(ConfigManager * configurationManager, evhttp_request *req){
	this->configurationManager = configurationManager;
	this->req = req;

	this->readCount = 0;
	this->writeCount = 0;
	this->numberOfDocumentsInIndex  = 0;
	this->docCount = 0;
}

/*
 * This function is always called by RoutingManager as the first call back function
 */
void GetInfoAggregatorAndPrint::preProcessing(ResultsAggregatorAndPrintMetadata metadata){

}
/*
 * This function is called by RoutingManager if a timeout happens, The call to
 * this function must be between preProcessing(...) and callBack()
 */
void GetInfoAggregatorAndPrint::timeoutProcessing(ShardId * shardInfo, SerializableGetInfoCommandInput * sentRequest,
		ResultsAggregatorAndPrintMetadata metadata){
	boost::unique_lock< boost::shared_mutex > lock(_access);
	messages << "{\"shard getInfo\":\"failed\",\"reason\":\"Corresponging shard ("<<
					shardInfo->toString()<<") timedout.\"}";
}


/*
 * The main function responsible of aggregating status (success or failure) results
 */
void GetInfoAggregatorAndPrint::callBack(vector<SerializableGetInfoResults *> responseObjects){
	boost::unique_lock< boost::shared_mutex > lock(_access);
	for(vector<SerializableGetInfoResults *>::iterator responseItr = responseObjects.begin();
			responseItr != responseObjects.end() ; ++responseItr){
		this->readCount += (*responseItr)->getReadCount();
		this->writeCount += (*responseItr)->getWriteCount();
		this->numberOfDocumentsInIndex = (*responseItr)->getNumberOfDocumentsInIndex();
		this->lastMergeTimeStrings.push_back((*responseItr)->getLastMergeTimeString());
		this->docCount += (*responseItr)->getDocCount();
		this->versionInfoStrings.push_back((*responseItr)->getVersionInfo());
	}
}

/*
 * The last call back function called by RoutingManager in all cases.
 * Example of call back call order for search :
 * 1. preProcessing()
 * 2. timeoutProcessing() [only if some shard times out]
 * 3. aggregateSearchResults()
 * 4. finalize()
 */
void GetInfoAggregatorAndPrint::finalize(ResultsAggregatorAndPrintMetadata metadata){

	//TODO : this print should be checked to make sure it prints correct json format
	std::stringstream str;
    str << "\"engine_status\":{";
    str << "\"search_requests\":\"" << this->readCount << "\",";
    str << "\"write_requests\":\"" <<  this->writeCount << "\",";
    str << "\"docs_in_index\":\"" << this->numberOfDocumentsInIndex << "\",";
    for(unsigned i=0; i < lastMergeTimeStrings.size() ; ++i){
		str << "\"shard_status\":{";
    	str << "\"last_merge\":\"" << this->lastMergeTimeStrings.at(i) << "\",";
    	str << "\"version\":\"" << this->versionInfoStrings.at(i) << "\"";
		str << "},";
    }
    str << "\"doc_count\":\"" << this->docCount << "\",";
    str << "}\n";
    str << "\"messages\":[" << messages.str() << "]\n";
    Logger::info("%s", messages.str().c_str());

    bmhelper_evhttp_send_reply(req, HTTP_OK, "OK",
            "{\"message\":\"The batch was processed successfully\",\"log\":["
                    + str.str() + "]}\n");
}



}
}

