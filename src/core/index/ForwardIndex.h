// $Id: ForwardIndex.h 3513 2013-06-29 00:27:49Z jamshid.esmaelnezhad $
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

 * Copyright © 2010 SRCH2 Inc. All rights reserved */

#pragma once
#ifndef __FORWARDINDEX_H__
#define __FORWARDINDEX_H__

#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/map.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

#include <instantsearch/Record.h>
#include "record/SchemaInternal.h"
#include "analyzer/AnalyzerInternal.h"
#include "util/cowvector/cowvector.h"
#include "util/Logger.h"
#include "util/encoding.h"
#include "util/half.h"
#include "util/VariableLengthAttributeContainer.h"
#include "instantsearch/TypedValue.h"
#include "util/mytime.h"
#include "util/ULEB128.h"
#include "thirdparty/snappy-1.0.4/snappy.h"
#include "util/ThreadSafeMap.h"

using std::vector;
using std::fstream;
using std::string;
using std::map;
using std::pair;
using half_float::half;
using namespace snappy;

// The upper bound of the number of keywords in a record is FFFFFF
#define KEYWORD_THRESHOLD ((1<<24) - 1)
// The upper bound of the number of sortable attributes in a record is FF
#define SORTABLE_ATTRIBUTES_THRESHOLD ((1<<8) - 1)

namespace srch2 {
namespace instantsearch {

typedef vector<pair<unsigned, pair<string, unsigned> > > KeywordIdKeywordStringInvertedListIdTriple;

// for reordering keyword ids
struct KeywordRichInformation {
    unsigned keywordId;
    float keywordScore;
    unsigned keywordAttribute;
    bool operator <(const KeywordRichInformation& keyword) const {
        return keywordId < keyword.keywordId;
    }
};

typedef pair<unsigned, pair<unsigned, unsigned> > NewKeywordIdKeywordOffsetTriple;
struct NewKeywordIdKeywordOffsetPairGreaterThan {
    bool operator()(const NewKeywordIdKeywordOffsetTriple &lhs,
            const NewKeywordIdKeywordOffsetTriple &rhs) const {
        return lhs.first > rhs.first;
    }
};

class ForwardList {
public:

    //getter and setter
    unsigned getNumberOfKeywords() const {
        return numberOfKeywords;
    }

    void setNumberOfKeywords(unsigned numberOfKeywords) {
        this->numberOfKeywords = numberOfKeywords;
    }

    float getRecordBoost() const {
        return recordBoost;
    }

    void setRecordBoost(float recordBoost) {
        this->recordBoost = recordBoost;
    }

    std::string getExternalRecordId() const {
        return externalRecordId;
    }

    void setExternalRecordId(std::string externalRecordId) {
        this->externalRecordId = externalRecordId;
    }

    std::string getInMemoryData() const {
        return inMemoryData;
    }

    void setInMemoryData(std::string inMemoryData) {
        this->inMemoryData = inMemoryData;
    }

    const std::string getRefiningAttributeValue(unsigned iter,
            const Schema * schema) const {
        return VariableLengthAttributeContainer::getAttribute(iter, schema, this->getRefiningAttributeValuesDataPointer());
    }

    const Byte * getRefiningAttributeContainerData() const {
        return getRefiningAttributeValuesDataPointer();
    }

    /*
     * The format of data in this array is :
     * ------------------------------------------------------------------------------------------------------------------
     * | keywordIDs | keywordRecordStaticScores | nonSearchableAttributeValues | keywordAttributeBitMap | positionIndex |
     * ------------------------------------------------------------------------------------------------------------------
     *
     * This function will calculate and prepare nonSearchableAttribute byte array in its place.
     * and will allocate the whole space and copy the last part (positional index)
     * the rest of data will be filled out through setKeywordId(...) , setKeywordRecordStaticScore(...)
     * and setKeywordAttributeBitmap(...) API calls.
     */
    void allocateSpaceAndSetNSAValuesAndPosIndex(const Schema * schema,
    		const vector<vector<string> > & nonSearchableAttributeValues,
    		bool shouldAttributeBitMapBeAllocated,
    		vector<uint8_t>& positionIndexDataVector){
    	this->nonSearchableAttributeValuesDataSize = VariableLengthAttributeContainer::getSizeNeededForAllocation(schema, nonSearchableAttributeValues);
        /////
    	this->positionIndexSize = positionIndexDataVector.size();
        //
    	// first two blocks are for keywordIDs and keywordRecordStaticScores.
    	dataSize = getKeywordIdsSizeInBytes() + getKeywordRecordStaticScoresSizeInBytes();
    	data = new Byte[dataSize +
    	                          this->nonSearchableAttributeValuesDataSize +
    	                          this->getKeywordAttributeBitmapsSizeInBytes() +
    	                          this->getPositionIndexSize()];
    	// next block is for nonSearchableAttributeValues
    	dataSize = dataSize + this->nonSearchableAttributeValuesDataSize;
    	// fourth block is attributeBitmap
    	/////
    	if(shouldAttributeBitMapBeAllocated == true){
			dataSize = dataSize + this->getKeywordAttributeBitmapsSizeInBytes();
    	}
    	// last part is positionIndex
    	/////
    	copy(positionIndexDataVector.begin() , positionIndexDataVector.end(), data + this->dataSize);
    	dataSize = dataSize + this->getPositionIndexSize();

    	// now that memory is allocated and position index is copied we can fill nonSearchableData in place.
    	VariableLengthAttributeContainer::fillWithoutAllocation(schema, nonSearchableAttributeValues, getRefiningAttributeValuesDataPointer() );
    }

    const unsigned* getKeywordIds() const {
        return getKeywordIdsPointer();
    }

    const unsigned getKeywordId(unsigned iter) const {
        return getKeywordIdsPointer()[iter];
    }

    void setKeywordId(unsigned iter, unsigned keywordId) {
        if (iter <= KEYWORD_THRESHOLD)
            this->getKeywordIdsPointer()[iter] = keywordId;
    }

    const float getKeywordRecordStaticScore(unsigned iter) const {
        return getKeywordRecordStaticScoresPointer()[iter];
    }

    void setKeywordRecordStaticScore(unsigned iter, float keywordScore) {
        if (iter <= KEYWORD_THRESHOLD)
            this->getKeywordRecordStaticScoresPointer()[iter] = keywordScore;
    }

    unsigned* getKeywordAttributeBitmaps() const {
        return getKeywordAttributeBitmapsPointer();
    }

    unsigned getKeywordAttributeBitmap(unsigned iter) const {
        return getKeywordAttributeBitmapsPointer()[iter];
    }

    void setKeywordAttributeBitmap(unsigned iter,
            unsigned keywordAttributeBitmap) {
        if (iter <= KEYWORD_THRESHOLD)
            this->getKeywordAttributeBitmapsPointer()[iter] = keywordAttributeBitmap;
    }

    //set the size of keywordIds and keywordRecordStaticScores to keywordListCapacity
    ForwardList(int keywordListCapacity = 0) {

        // we consider at most KEYWORD_THRESHOLD keywords
        if (keywordListCapacity > KEYWORD_THRESHOLD)
            keywordListCapacity = KEYWORD_THRESHOLD;
        numberOfKeywords = 0;
        recordBoost = 0.5;
        inMemoryData = "";
        // the dataSize and data are initialized temporarily. They will actually be initialized in
        // allocateSpaceAndSetNSAValuesAndPosIndex when other pieces of data are also ready.
        dataSize = 0;
        data = NULL;
        nonSearchableAttributeValuesDataSize = 0;
        positionIndexSize = 0;
    }

    virtual ~ForwardList() {
        if(data != NULL){
        	delete data;
        }
    }

    float computeFieldBoostSummation(const Schema *schema,
            const TokenAttributeHits &hits) const;

    //unsigned getForwardListElement(unsigned cursor) const;

    TypedValue getForwardListRefiningAttributeTypedValue(
            const SchemaInternal* schemaInternal,
            unsigned schemaNonSearchableAttributeId) const;

    bool haveWordInRangeWithStemmer(const SchemaInternal* schema,
            const unsigned minId, const unsigned maxId,
            const unsigned termSearchableAttributeIdToFilterTermHits,
            unsigned &matchingKeywordId,
            unsigned &matchingKeywordAttributeBitmap,
            float &matchingKeywordRecordStaticScore, bool &isStemmed) const;
    bool haveWordInRange(const SchemaInternal* schema, const unsigned minId,
            const unsigned maxId,
            const unsigned termSearchableAttributeIdToFilterTermHits,
            unsigned &keywordId, unsigned &termAttributeBitmap,
            float &termRecordStaticScore) const;

    unsigned getKeywordOffset(unsigned keywordId) const;

    bool getWordsInRange(const SchemaInternal* schema, const unsigned minId,
            const unsigned maxId,
            const unsigned termSearchableAttributeIdToFilterTermHits,
            vector<unsigned> &keywordIdsVector) const;

    /**************************PositionIndex****************/
    float getTermRecordStaticScore(unsigned forwardIndexId,
            unsigned keywordOffset) const;
    /**
     * Input @param tokenAttributeHitsMap is a map of keywordTokens to a "vector of positions" for the given record.
     * The PositionIndex is ZERO terminated, i.e. ZERO is the end flag for each position list.
     * So, keyword positions in each record attribute start from 1.
     * For example,
     * Consider a record, a1 a2 a1 a1 a3 a1 a3
     * keywordIdMap has the following key,value pairs:
     * a1: [1, 3, 4, 7]
     * a2: [2]
     * a3: [6, 8]
     * Here, a1,a2,a3 are keywordTokesn.
     *
     * Internally, addRecord iterates over the keys and for each key, the "positions" are first appended and then a [0] is appended to the positionVector.
     * In the above case, for key a1, we append [1, 3, 4, 7, 0].
     * for key a2, we append [2, 0].
     * for key a3, we append [6, 8, 0].
     */

    bool isValidRecordTermHit(const SchemaInternal *schema,
            unsigned keywordOffset, unsigned searchableAttributeId,
            unsigned &termAttributeBitVec, float& termRecordStaticScore) const;
    bool isValidRecordTermHitWithStemmer(const SchemaInternal *schema,
            unsigned keywordOffset, unsigned searchableAttributeId,
            unsigned &matchingKeywordAttributeBitmap,
            float &termRecordStaticScore, bool &isStemmed) const;

    unsigned getNumberOfBytes() const;


    //void mapOldIdsToNewIds();

    // Position Indexes APIs
    void getKeyWordPostionsInRecordField(unsigned keywordId, unsigned attributeId,
    		unsigned attributeBitMap, vector<unsigned>& positionList) const;

private:
    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
        typename Archive::is_loading load;
        ar & this->numberOfKeywords;
        ar & this->recordBoost;
        ar & this->nonSearchableAttributeValuesDataSize;
        ar & this->positionIndexSize;
        ar & this->dataSize;
        /*
         * Since we don't have access to ForwardIndex and we don't know whether attributeBasedSearch is on, our encodin
         * scheme is :
         * first save/load the size of keywordAttributeBitmaps array, and then if size was not zero also save/load the array itself.
         *
         */
        // In loading process, we need to allocate space for the members first.
        if (load) {
        	this->data = new Byte[this->dataSize];
        }
        ar
                & boost::serialization::make_array(this->data,
                        this->dataSize);
        ar & this->externalRecordId;
        ar & this->inMemoryData;
    }

    // members
    unsigned numberOfKeywords;
    half recordBoost;
    std::string externalRecordId;
    std::string inMemoryData;


    /*
     * The format of data in this array is :
     * ------------------------------------------------------------------------------------------------------------------
     * | keywordIDs | keywordRecordStaticScores | nonSearchableAttributeValues | keywordAttributeBitMap | positionIndex |
     * ------------------------------------------------------------------------------------------------------------------
     */
    Byte * data;

    unsigned nonSearchableAttributeValuesDataSize;
    unsigned positionIndexSize;
    unsigned dataSize;


    ///////////////////     Keyword IDs Helper Functions //////////////////////////////////////
    inline unsigned * getKeywordIdsPointer() const{
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordRecordStaticScores | nonSearchableAttributeValues | keywordAttributeBitMap | positionIndex |
         * ------------------------------------------------------------------------------------------------------------------
         */
    	// keyword IDs start from the 0th position.
    	return (unsigned *)(data + 0);
    }
    inline unsigned getKeywordIdsSizeInBytes(unsigned sizeInUnsigned) const {

    	return sizeInUnsigned * (sizeof(unsigned) / sizeof(Byte));
    }
    inline unsigned getKeywordIdsSizeInBytes() const {
    	return getKeywordIdsSizeInBytes(this->getNumberOfKeywords());
    }

    ////////////////////  Keyword Record Static Scores Helper Fucntions /////////////////////////
    inline half* getKeywordRecordStaticScoresPointer() const{
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordRecordStaticScores | nonSearchableAttributeValues | keywordAttributeBitMap | positionIndex |
         * ------------------------------------------------------------------------------------------------------------------
         */
    	return (half *)(data + getKeywordIdsSizeInBytes());
    }
    inline unsigned getKeywordRecordStaticScoresSizeInBytes(unsigned sizeInHalf) const {
    	return sizeInHalf * (sizeof(half) / sizeof(Byte));
    }
    inline unsigned getKeywordRecordStaticScoresSizeInBytes() const {
    	return getKeywordRecordStaticScoresSizeInBytes(this->getNumberOfKeywords());
    }


    //////////////// Non Searchable Attribute Values Helper Function /////////////////////////
    inline Byte * getRefiningAttributeValuesDataPointer() const{
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordRecordStaticScores | nonSearchableAttributeValues | keywordAttributeBitMap | positionIndex |
         * ------------------------------------------------------------------------------------------------------------------
         */
    	return data +
    			getKeywordIdsSizeInBytes() +
    			getKeywordRecordStaticScoresSizeInBytes();
    }
    inline unsigned getNonSearchableAttributeValuesDataSize() const{
    	return this->nonSearchableAttributeValuesDataSize;
    }


    //////////////////// Keyword Attributes Bitmap Helper Functions ////////////////////////////
    inline unsigned * getKeywordAttributeBitmapsPointer() const{
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordRecordStaticScores | nonSearchableAttributeValues | keywordAttributeBitMap | positionIndex |
         * ------------------------------------------------------------------------------------------------------------------
         */
    	return (unsigned *)(data +
    			getKeywordIdsSizeInBytes() +
    			getKeywordRecordStaticScoresSizeInBytes() +
    			getNonSearchableAttributeValuesDataSize());
    }
    inline unsigned getKeywordAttributeBitmapsSizeInBytes(unsigned sizeInUnsigned) const{
    	return sizeInUnsigned * (sizeof(unsigned) / sizeof(Byte));
    }
    inline unsigned getKeywordAttributeBitmapsSizeInBytes() const{
    	return getKeywordAttributeBitmapsSizeInBytes(this->getNumberOfKeywords());
    }

    /////////////////////// Position Index Helper Functions //////////////////////////////
    inline uint8_t * getPositionIndexPointer() const{
        /*
         * The format of data in this array is :
         * ------------------------------------------------------------------------------------------------------------------
         * | keywordIDs | keywordRecordStaticScores | nonSearchableAttributeValues | keywordAttributeBitMap | positionIndex |
         * ------------------------------------------------------------------------------------------------------------------
         */
    	return (uint8_t *)(data +
    			getKeywordIdsSizeInBytes() +
    			getKeywordRecordStaticScoresSizeInBytes() +
    			getNonSearchableAttributeValuesDataSize() +
    			getKeywordAttributeBitmapsSizeInBytes());
    }
    inline unsigned getPositionIndexSize(){
    	return positionIndexSize;
    }




};

typedef std::pair<ForwardList*, bool> ForwardListPtr;

/**
 * A forward index includes a forward list for each record.  The list (sorted)
 * includes the keywords in the record, represented as keywordIds, representing the trie leaf node
 * corresponding to the keyword.
 */
class ForwardIndex {
private:
    const static unsigned MAX_SCORE = unsigned(-1);

    ///vector of forwardLists, where RecordId is the element index.
    cowvector<ForwardListPtr> *forwardListDirectory;

    //Used only in WriteView
    ThreadSafeMap<std::string, unsigned> externalToInternalRecordIdMap;

    // Build phase structure
    // Stores the order of records, by which it was added to forward index. Used in bulk initial insert
    //std::vector<unsigned> recordOrder;

    // Set to true when the commit function is called. Set to false, when the addRecord function is called.
    bool commited_WriteView;
    bool mergeRequired;

    // Initialised in constructor and used in calculation of offset in filterAttributesVector. This is lighter than serialising the schema itself.
    const SchemaInternal *schemaInternal;

    friend class boost::serialization::access;


    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & forwardListDirectory;
        ar & externalToInternalRecordIdMap;
        ar & commited_WriteView;
    }

    //helper functions
    void _getPositionListFromTokenAttributesMap(
            KeywordIdKeywordStringInvertedListIdTriple &keywordIdList,
            map<string, TokenAttributeHits> &tokenAttributeHitsMap,
            vector<unsigned>& positionList);

public:

    // is attribute based search or not
    bool isAttributeBasedSearch;

    ForwardIndex(const SchemaInternal* schemaInternal);
    ForwardIndex(const SchemaInternal* schemaInternal,
            unsigned expectedNumberOfDocumentsToInitialize);
    virtual ~ForwardIndex();

    unsigned getTotalNumberOfForwardLists_ReadView() const;
    unsigned getTotalNumberOfForwardLists_WriteView() const;

    void addDummyFirstRecord(); // For Trie bootstrap

    /**
     * Add a new record, represented by a list of keyword IDs. The keywordIds are unique and have no order.
     * If the record ID already exists, do nothing. Else, add the record.
     */
    void addRecord(const Record *record, const unsigned recordId,
            KeywordIdKeywordStringInvertedListIdTriple &keywordIdList,
            map<string, TokenAttributeHits> &tokenAttributeHitsMap);

    /**
     * Set the deletedFlag on the forwardList, representing record deletion.
     */
    void setDeleteFlag(unsigned internalRecordId);

    /**
     * Resume the deletedFlag on the forwardList, reversing record deletion.
     */
    void resetDeleteFlag(unsigned internalRecordId);

    /**
     * Verify if given interval is present in the forward list
     */
    //bool havePrefixInForwardList(const unsigned recordId, const unsigned minId, const unsigned maxId, const int termSearchableAttributeIdToFilterTermHits, float &score) const;
    /**
     * Check if the ForwardList of recordId has a keywordId in range [minId, maxId]. Note that this is closed range.
     * If the recordId does not exist, return false.
     */
    ///Added for stemmer
    bool haveWordInRangeWithStemmer(const unsigned recordId,
            const unsigned minId, const unsigned maxId,
            const unsigned termSearchableAttributeIdToFilterTermHits,
            unsigned &matchingKeywordId,
            unsigned &matchingKeywordAttributeBitmap,
            float &matchingKeywordRecordStaticScore, bool &isStemmed) const;
    bool haveWordInRange(const unsigned recordId, const ForwardList* fl, const unsigned minId,
            const unsigned maxId,
            const unsigned termSearchableAttributeIdToFilterTermHits,
            unsigned &matchingKeywordId,
            unsigned &matchingKeywordAttributeBitmap,
            float &matchingKeywordRecordStaticScore) const;

    /**
     * Check if the ForwardList of recordId has a keywordId in range [minId, maxId]. Note that this is closed range.
     * If the recordId does not exist, return false. - STEMMER VERSION
     */
    //bool haveWordInRangeWithStemmer(const unsigned recordId, const unsigned minId, const unsigned maxId, const int termSearchableAttributeIdToFilterTermHits, unsigned &keywordId, float &termRecordStaticScore, bool& isStemmed);
    /**
     * Returns the number of bytes occupied by the ForwardIndex
     */
    unsigned getNumberOfBytes() const;

    static void exportData(ForwardIndex &forwardIndex, const string &exportedDataFileName);

    /**
     * Build Phase functions
     */
    const ForwardList *getForwardList(unsigned recordId, bool &valid) const;
    //ForwardList *getForwardListToChange(unsigned recordId, bool &valid); // CHENLI
    ForwardList *getForwardList_ForCommit(unsigned recordId);

    //ForwardListElement *getForwardListElementByOffset(unsigned forwardIndexOffset);

    /** COMMITING FORWARDINDEX
     * 1. Get the invertedIndex.
     * 2. Set all hitCounts in ForwardIndexDirectory to zero.
     * 3. For each invertedListDirectory element -> keywordId, get newKeywordId, i.e
     *      1. For each RecordId in Inverted List
     *          1. Insert into forwardIndex using the following steps.
     *              1. Look up ForwardIndexDirectory and get offset, hitcount.
     *              2. Insert at offset+hitcount, the newKeywordId
     *              3. Increment hitcount
     * 4. Sort each ForwardList based on newKeywordIds
     */
    void commit();
    void merge();
    //void commit(ForwardList *forwardList, const vector<unsigned> *oldIdToNewIdMap, vector<NewKeywordIdKeywordOffsetTriple> &forwardListReOrderAtCommit );
    void commit(ForwardList *forwardList,
            const map<unsigned, unsigned> &oldIdToNewIdMapper,
            vector<NewKeywordIdKeywordOffsetTriple> &forwardListReOrderAtCommit);

    void reorderForwardList(ForwardList *forwardList,
            const map<unsigned, unsigned> &oldIdToNewIdMapper,
            vector<NewKeywordIdKeywordOffsetTriple> &forwardListReOrderAtCommit);
    void finalCommit();
    //void commit(const std::vector<unsigned> *oldIdToNewIdMap);

    bool isCommitted() const {
        return this->commited_WriteView;
    }
    bool isMergeRequired() const { return mergeRequired; }
    /*std::vector<unsigned>* getRecordOrderVector() {
     return &recordOrder;
     };*/

    void setSchema(SchemaInternal *schema) {
        this->schemaInternal = schema;
    }

    const SchemaInternal *getSchema() const {
        return this->schemaInternal;
    }

    //void print_test() const;
    void print_test();
    void print_size() const;

    float getTermRecordStaticScore(unsigned forwardIndexId,
            unsigned keywordOffset) const;
    /**
     * Input @param tokenAttributeHitsMap is a map of keywordTokens to a "vector of positions" for the given record.
     * The PositionIndex is ZERO terminated, i.e. ZERO is the end flag for each position list.
     * So, keyword positions in each record attribute start from 1.
     * For example,
     * Consider a record, a1 a2 a1 a1 a3 a1 a3
     * keywordIdMap has the following key,value pairs:
     * a1: [1, 3, 4, 7]
     * a2: [2]
     * a3: [6, 8]
     * Here, a1,a2,a3 are keywordTokesn.
     *
     * Internally, addRecord iterates over the keys and for each key, the "positions" are first appended and then a [0] is appended to the positionVector.
     * In the above case, for key a1, we append [1, 3, 4, 7, 0].
     * for key a2, we append [2, 0].
     * for key a3, we append [6, 8, 0].
     */
    bool isValidRecordTermHit(unsigned forwardIndexId, unsigned keywordOffset,
            unsigned searchableAttributeId, unsigned &termAttributeBitmap,
            float& termRecordStaticScore) const;
    bool isValidRecordTermHitWithStemmer(unsigned forwardIndexId,
            unsigned keywordOffset, unsigned searchableAttributeId,
            unsigned &matchingKeywordAttributeBitmap,
            float &matchingKeywordRecordStaticScore, bool &isStemmed) const;

    unsigned getKeywordOffset(unsigned forwardListId, unsigned keywordId) const;

    /*****-record-id-converter*****/
    /**
     * Append @param[in] externalRecordId to the RecordIdVector
     * @param[out] internalRecordId is the internal recordID corresponding to externalRecordId.
     * Internally, internalRecordId is the uint value of the position where the externalRecordId was appended, i.e, "RecordIdVector.size() - 1".
     *
     * If the externalRecordId already exists, "false" is returned. internalRecordId is set to a default (unsigned)(-1) in this case.
     */
    //bool appendExternalRecordId(unsigned externalRecordId, unsigned &internalRecordId);// Goes to both map-write and forwardIndex
    void appendExternalRecordIdToIdMap(const std::string &externalRecordId,
            unsigned &internalRecordId);

    bool deleteRecord(const std::string &externalRecordId);
    bool deleteRecordGetInternalId(
            const std::string &externalRecordId, unsigned &internalRecordId);
    //bool deleteExternalRecordId(unsigned externalRecordId); // Goes to forwardIndex and map-write

    bool recoverRecord(const std::string &externalRecordId,
            unsigned internalRecordId);

    INDEXLOOKUP_RETVAL lookupRecord(
            const std::string &externalRecordId) const;

    void reassignKeywordIds(const unsigned recordId,
            const map<unsigned, unsigned> &keywordIdMapper);
    //void reassignKeywordIds(map<unsigned, unsigned> &reassignedKeywordIdMapper);

    /**
     * For the given internalRecordId, returns the ExternalRecordId. ASSERT if the internalRecordId
     * is out of bound of RecordIdVector.
     */
    bool getExternalRecordIdFromInternalRecordId(const unsigned internalRecordId,
            std::string &externalRecordId) const; // Goes to forwardIndex-read

    bool getInternalRecordIdFromExternalRecordId(const std::string &externalRecordId,
            unsigned &internalRecordId) const; // Goes to recordIdConverterMap-read

    /**
     * Access the InMemoryData of a record using InternalRecordId
     */
    std::string getInMemoryData(unsigned internalRecordId) const;

    void convertToVarLengthArray(const vector<unsigned>& positionListVector,
    							 vector<uint8_t>& grandBuffer);

};

}
}

#endif //__FORWARDINDEX_H__
