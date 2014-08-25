#ifndef __SHARDING_PROCESSOR_SEARCH_RESULTS_AGGREGATOR_AND_PRINT_H__
#define __SHARDING_PROCESSOR_SEARCH_RESULTS_AGGREGATOR_AND_PRINT_H__

#include "sharding/processor/aggregators/DistributedProcessorAggregator.h"
#include "../serializables/SerializableSearchResults.h"
#include "../serializables/SerializableSearchCommandInput.h"
#include <event2/http.h>
#include "wrapper/ParsedParameterContainer.h"

namespace srch2is = srch2::instantsearch;
using namespace std;

using namespace srch2is;

namespace srch2 {
namespace httpwrapper {

class SearchResultsAggregator : public DistributedProcessorAggregator<SearchCommand , SearchCommandResults> {

public:

    /*
     * This class is needed to safely point to a string in an entry of a map
     * We use this wrapper around pointer to avoid copy query result strings
     */
    class MapStringPtr{
    public:
        MapStringPtr(const map<string, std::pair<string, RecordSnippet> >::const_iterator itr){
            this->itr = itr;
        }

        const string * operator->() const{
            return &(itr->second.first);
        }
        const string & operator*() const{
            return itr->second.first;
        }

        const RecordSnippet& getRecordSnippet() const{
        	return (itr->second.second);
        }

    private:
        map<string, std::pair<string, RecordSnippet> >::const_iterator itr;
    };

    class AggregatedQueryResults{
    public:
        vector<pair< QueryResult *, MapStringPtr> > allResults;
        std::map<std::string, std::pair< FacetType , std::vector<std::pair<std::string, float> > > > aggregatedFacetResults;
        unsigned aggregatedEstimatedNumberOfResults;
        bool isResultsApproximated;
        unsigned aggregatedSearcherTime;

        AggregatedQueryResults(){
            aggregatedEstimatedNumberOfResults = 0;
            isResultsApproximated = false;
            aggregatedSearcherTime = 0;
        }
    };


    SearchResultsAggregator(ConfigManager * configurationManager, evhttp_request *req, boost::shared_ptr<const ClusterResourceMetadata_Readview> clusterReadview, unsigned coreId);
    LogicalPlan & getLogicalPlan();
    ParsedParameterContainer * getParamContainer();

    void setParsingValidatingRewritingTime(unsigned time);

    unsigned getParsingValidatingRewritingTime();

    struct timespec & getStartTimer();
    /*
     * This function is always called by RoutingManager as the first call back function
     */
    void preProcess(ResponseAggregatorMetadata metadata){};
    /*
     * This function is called by RoutingManager if a timeout happens, The call to
     * this function must be between preProcessing(...) and callBack()
     */
    void processTimeout(PendingMessage<SearchCommand,
            SearchCommandResults> * message,
            ResponseAggregatorMetadata metadata);


    /*
     * The main function responsible of aggregating search results
     * this function uses aggregateRecords and aggregateFacets for
     * aggregating result records and calculated records
     */
    void callBack(vector<PendingMessage<SearchCommand,
            SearchCommandResults> * > messages);

    void callBack(PendingMessage<SearchCommand, SearchCommandResults> * message){};

    /*
     * The last call back function called by RoutingManager in all cases.
     * Example of call back call order for search :
     * 1. preProcessing()
     * 2. timeoutProcessing() [only if some shard times out]
     * 3. aggregateSearchResults()
     * 4. finalize()
     */
    void finalize(ResponseAggregatorMetadata metadata){
        // to protect messages
        boost::unique_lock< boost::shared_mutex > lock(_access);
        // print the results
        printResults();
    }


    void printResults();

    /**
     * Iterate over the recordIDs in queryResults and get the record.
     * Add the record information to the request.out string.
     */
    void printResults(evhttp_request *req,
            const evkeyvalq &headers, const LogicalPlan &queryPlan,
            const CoreInfo_t *indexDataConfig,
            const vector<pair< QueryResult *, MapStringPtr> > allResults,
            const Query *query,
            const unsigned start, const unsigned end,
            const unsigned retrievedResults, const string & message,
            const unsigned ts1, const vector<RecordSnippet>& recordSnippets, unsigned hlTime, bool onlyFacets) ;

    void printOneResultRetrievedById(evhttp_request *req, const evkeyvalq &headers,
            const LogicalPlan &queryPlan,
            const CoreInfo_t *indexDataConfig,
            const vector<pair< QueryResult *, MapStringPtr> > allResults,
            const string & message,
            const unsigned ts1);

    void genRecordJsonString(const srch2::instantsearch::Schema * schema, StoredRecordBuffer buffer,
            const string& extrnalRecordId, string& sbuffer);
    void genRecordJsonString(const srch2::instantsearch::Schema * schema, StoredRecordBuffer buffer,
            const string& externalRecordId, string& sbuffer, const vector<string>* attrToReturn);

    void cleanAndAppendToBuffer(const string& in, string& out);
    void genSnippetJSONString(const RecordSnippet& recordSnippet, string& sbuffer);

    class QueryResultsComparatorOnlyScore{
    public:
        bool operator()(const pair< QueryResult *, MapStringPtr> & left, const pair< QueryResult *, MapStringPtr> &  right){
            return (left.first->getResultScore() > right.first->getResultScore());
        }
    };

private:

    /*
     * Combines the results coming from all shards and
     * resorts them based on their scores
     */
    void aggregateRecords();

    /*
     * Combines the facet results coming from all shards and
     * re-calculates facet values
     */
    void aggregateFacets();


    /*
     * Merges destination with source and adds new items to source
     */
    void mergeFacetVectors(std::vector<std::pair<std::string, float> > & source,
            const std::vector<std::pair<std::string, float> > & destination);


    /*
     * Aggregates the estimatedNumberOfResults from all responses
     * and also sets resultsApproximated if any of the responses has this flag set
     */
    void aggregateEstimations();


    /*
     * This vector will be populated from QueryResults coming from all shards
     * If some shards fail or don't return any results we ignore them
     * Search should not be blocking in failure case
     */
    vector<pair< QueryResults *, const map<string, std::pair<string, RecordSnippet> > * > > resultsOfAllShards;

    /*
     * This variable contains the final aggregated results
     */
    SearchResultsAggregator::AggregatedQueryResults results;
    ConfigManager * configurationManager;
    evhttp_request *req;

    LogicalPlan logicalPlan;
    unsigned parsingValidatingRewritingTime;
    struct timespec tstart;
    ParsedParameterContainer paramContainer;

    // these members can be accessed concurrently by multiple threads
    mutable boost::shared_mutex _access;
    std::stringstream messages;
};


}
}


#endif // __SHARDING_PROCESSOR_SEARCH_RESULTS_AGGREGATOR_AND_PRINT_H_