// $Id$ 07/11/13 Jamshid


/*
 * The Software is made available solely for use according to the License Agreement. Any reproduction
 * or redistribution of the Software not in accordance with the License Agreement is expressly prohibited
 * by law, and may result in severe civil and criminal penalties. Violators will be prosecuted to the
 * maximum extent possible.
 *
 * THE SOFTWARE IS WARRANTED, IF AT ALL, ONLY ACCORDING TO THE TERMS OF THE LICENSE AGREEMENT. EXCEPT
 * AS WARRANTED IN THE LICENSE AGREEMENT, SRCH2 INC. HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS WITH
 * REGARD TO THE SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES AND CONDITIONS OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT.  IN NO EVENT SHALL SRCH2 INC. BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF SOFTWARE.

 * Copyright © 2013 SRCH2 Inc. All rights reserved
 */


#ifndef _WRAPPER_QUERYPLEANGENERATOR_H_
#define _WRAPPER_QUERYPLEANGENERATOR_H_

#include "instantsearch/ResultsPostProcessor.h"
#include "ParsedParameterContainer.h"

using srch2::instantsearch::ResultsPostProcessorPlan;
using srch2::instantsearch::Query;

using namespace std;
namespace srch2{

namespace httpwrapper{

class QueryPlan
{
public:


	QueryPlan(const ParsedParameterContainer & paramsContainer){
		this->paramsContainer = paramsContainer;
		exactQuery = NULL;
		fuzzyQuery = NULL;
	}
	~QueryPlan(){
		if(exactQuery != NULL) delete exactQuery;

		if(fuzzyQuery != NULL) delete fuzzyQuery;

		if(postProcessingPlan != NULL) delete postProcessingPlan;

	}
	Query* getExactQuery() const {
		return exactQuery;
	}

	void setExactQuery(Query* exactQuery) { // TODO : change the header
		// it gets enough information from the arguments and allocates the query objects
		this->exactQuery = exactQuery;
	}

	Query* getFuzzyQuery() const {
		return fuzzyQuery;
	}

	void setFuzzyQuery(Query* fuzzyQuery) { // TODO : change the header

		// it gets enough information from the arguments and allocates the query objects
		this->fuzzyQuery = fuzzyQuery;
	}


	/*
	 * 1. creates exact and fuzzy queries
	 * 2. Generates the post processing plan
	 */
	void generatePlan(){

		// create query objects
		createExactAndFuzzyQueries();
		// generate post processing plan
		createPostProcessingPlan();
	}




private:
	const ParsedParameterContainer & paramsContainer;
	Query *exactQuery;
	Query *fuzzyQuery;

	ResultsPostProcessorPlan * postProcessingPlan;


	/// Plan related information


	///

	// creates a post processing plan based on information from Query
	void createPostProcessingPlan(){

		// NOTE: FacetedSearchFilter should be always the last filter.
		// this function goes through the summary and uses the members of parsedInfo to fill out the query objects and
		// also create and set the plan
	}

	void createExactAndFuzzyQueries(){

	}

};
}
}

#endif // _WRAPPER_QUERYPLEANGENERATOR_H_