
// $Id: InvertedIndex.h 3480 2013-06-19 08:00:34Z jiaying $

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

 * Copyright © 2010 SRCH2 Inc. All rights reserved
 */

#pragma once
#ifndef __INVERTEDINDEX_H__
#define __INVERTEDINDEX_H__

#include "util/cowvector/cowvector.h"
#include "index/ForwardIndex.h"
//#include <instantsearch/Schema.h>
#include <instantsearch/Ranker.h>
#include "util/Assert.h"
#include "util/Log.h"
#include "util/RankerExpression.h"
#include "index/InvertedListElement.h"
#include "util/cowvector/compression/cowvector_S16.h"

#include <fstream>
#include <algorithm>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <set>

using std::map;
using std::set;
using std::vector;
using std::string;
using std::ifstream;
using std::ofstream;

namespace srch2
{
namespace instantsearch
{

class InvertedListContainer
{
private:
    // Comparator
    class InvertedListElementGreaterThan
    {
    private:
        const ForwardIndex* forwardIndex;
    public:
        InvertedListElementGreaterThan(const ForwardIndex* forwardIndex) {
            this->forwardIndex = forwardIndex;
        }

        // this operator should be consistent with two others in TermVirtualList.h and QueryResultsInternal.h
        bool operator() (const InvertedListElement &lhs, const InvertedListElement &rhs) const
        {
            float leftRecordScore  = this->forwardIndex->getTermRecordStaticScore(lhs.recordId, lhs.positionIndexOffset);
            float rightRecordScore = this->forwardIndex->getTermRecordStaticScore(rhs.recordId, rhs.positionIndexOffset);
            return DefaultTopKRanker::compareRecordsGreaterThan(leftRecordScore,  lhs.recordId,
                                                                rightRecordScore, rhs.recordId);
        }
    };

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & invList;
    }

public:

    //CowInvertedList *invList;
    cowvector<unsigned> *invList;

    InvertedListContainer() // TODO for serialization. Remove dependancy
    {
    	this->invList = new cowvector<unsigned>;
    };

    InvertedListContainer(unsigned capacity)
    {
        this->invList = new cowvector<unsigned>(capacity);
    };

    virtual ~InvertedListContainer()
    {
        delete invList;
    };

    const unsigned getInvertedListElement(unsigned index) const
    {
        ts_shared_ptr<vectorview<unsigned> > readView;
        this->invList->getReadView(readView);
        return readView->getElement(index);
    };

    unsigned getReadViewSize() const
    {
        ts_shared_ptr<vectorview<unsigned> > readView;
        this->invList->getReadView(readView);
        return readView->size();
    };

    unsigned getWriteViewSize() const
    {
        ts_shared_ptr<vectorview<unsigned> > writeView;
        this->invList->getWriteView(writeView);
        return writeView->size();
    };

    void setInvertedListElement(unsigned index, unsigned recordId)
    {
        ts_shared_ptr<vectorview<unsigned> > writeView;
        this->invList->getWriteView(writeView);
        writeView->at(index) = recordId;
    };

    void addInvertedListElement(unsigned recordId)
    {
        ts_shared_ptr<vectorview<unsigned> > writeView;
        this->invList->getWriteView(writeView);
        writeView->push_back(recordId);
    };

    void sortAndMergeBeforeCommit(const unsigned keywordId, const ForwardIndex *forwardIndex, bool needToSortEachInvertedList);

    void sortAndMerge(const unsigned keywordId, const ForwardIndex *forwardIndex);
};

typedef InvertedListContainer* InvertedListContainerPtr;

class InvertedIndex
{
public:

    //InvertedIndex(PositionIndex *positionIndex);
    InvertedIndex(ForwardIndex *forwardIndex);

    virtual ~InvertedIndex();

    /**
     *
     * Reader Functions called by IndexSearcherInternal and TermVirtualList
     *
     */

    /* Gets the @param invertedListElement in the @param invertedList. Internally, we first get the sub-sequence of invertedIndexVector represented by
     * [invertedList.offset,invertedList.currentHitCount] and return the InvertedListElement at invertedList.offset + cursor.
     * We make assertions to check if offset, offset + currentHitCount is within getTotalLengthOfInvertedLists(). Also, assert to check if currentHitCount > cursor.
     */
    const unsigned getInvertedListElementByDirectory(const unsigned invertedListId, const unsigned cursor) const;
    unsigned getInvertedListSize_ReadView(const unsigned invertedListId) const;
    bool isValidTermPositionHit(unsigned forwardListId, unsigned keywordOffset, 
                    unsigned searchableAttributeId, unsigned& termAttributeBitmap, float &termRecordStaticScore) const;

    unsigned getKeywordOffset(unsigned forwardListId, unsigned invertedListOffset) const;

    /**
     * Writer Functions called by IndexerInternal
     */
    void incrementHitCount(unsigned invertedIndexDirectoryIndex);
    void incrementDummyHitCount(unsigned invertedIndexDirectoryIndex);// For Trie Bootstrap
    void addRecord(ForwardList* forwardList, RankerExpression *rankerExpression,
            const unsigned forwardListOffset, const SchemaInternal *schema,
            const Record *record, const unsigned totalNumberOfDocuments, const KeywordIdKeywordStringInvertedListIdTriple &keywordIdList);

    void initialiseInvertedIndexCommit();
    void commit( ForwardList *forwardList, RankerExpression *rankerExpression,
            const unsigned forwardListOffset, const unsigned totalNumberOfDocuments,
            const Schema *schema, const vector<NewKeywordIdKeywordOffsetTriple> &newKeywordIdKeywordOffsetTriple);

    // When we construct the inverted index from a set of records, in the commit phase we need to sort each inverted list,
    // i.e., needToSortEachInvertedList = true.
	// When we load the inverted index from disk, we do NOT need to sort each inverted list since it's already sorted,
    // i.e., needToSortEachInvertedList = false.
    void finalCommit(bool needToSortEachInvertedList = true);
    void merge();

    void setForwardIndex(ForwardIndex *forwardIndex) {
        this->forwardIndex = forwardIndex;
    }

    /**
     * Adds a @param invertedListElement to the @param invertedList. Internally, we first get the sub-sequence of invertedIndexVector represented by
     * [invertedList.offset,invertedList.currentHitCount] and scan for the first InvertedListElement = {0,0}, and then insert @param invertedListElement to it.
     * When we do not find an empty space (which is unlikely in our implementation of index construction), we raise a assertion panic.
     */
    void  addInvertedListElement(unsigned invertedListDirectoryPosition, unsigned recordId);
    unsigned getTotalNumberOfInvertedLists_ReadView() const;

    /**
     * Prints the invertedIndexVector. Used for testing purposes.
     */
    void print_test() const;
    int getNumberOfBytes() const;

    static void load(InvertedIndex &invertedIndex, const string &invertedIndexFullPathFileName)
    {
        // read the InvertedIndex from the file
        ifstream ifs(invertedIndexFullPathFileName.c_str(), std::ios::binary);
        boost::archive::binary_iarchive ia(ifs);
        ia >> invertedIndex;
        ifs.close();
        // after we load the InvertedIndex from the disk to the memory,
        // the readview and writeview of each InvertedList and the InvertedIndexVector are not separated.
        // so we need to call finalCommit() to separate them.
        // We do not need to sort each inverted list since it's already sorted.  So we pass a "false" flag.
        invertedIndex.finalCommit(false);
     };

    static void save(const InvertedIndex &invertedIndex, const string &invertedIndexFullPathFileName)
    {
        ofstream ofs(invertedIndexFullPathFileName.c_str(), std::ios::binary);
        boost::archive::binary_oarchive oa(ofs);
        oa << invertedIndex;
        ofs.close();
    };


    void printInvList(const unsigned invertedListId) const
    {
        ts_shared_ptr<vectorview<InvertedListContainerPtr> > readView;
        this->invertedIndexVector->getReadView(readView);

        unsigned readViewListSize = readView->getElement(invertedListId)->getReadViewSize();

        std::cerr << "Print invListid:" << invertedListId << "|size:" << readViewListSize << std::endl;
        InvertedListElement invertedListElement;
        for (unsigned i = 0; i < readViewListSize; i++)
        {
        	readView->getElement(invertedListId)->getInvertedListElement(i);
            unsigned recordId = invertedListElement.recordId;
            unsigned positionIndexOffset = invertedListElement.positionIndexOffset;
            float score;
            unsigned termAttributeBitmap;
            if (isValidTermPositionHit(recordId, positionIndexOffset, -1, termAttributeBitmap, score) )
                std::cerr << recordId  << "|" << positionIndexOffset << "|" << score <<  std::endl;
        }
    };
    cowvector<unsigned> *getKeywordIds()
	{
    	return keywordIds;
	}

private:

    float getIdf(const unsigned totalNumberOfDocuments, const unsigned keywordId) const;
    float computeRecordStaticScore(RankerExpression *rankerExpression, const float recordBoost,
                       const float recordLength, const float tf, const float idf,
                       const float sumOfFieldBoosts) const;

    cowvector<InvertedListContainerPtr> *invertedIndexVector;

    //mapping from keywordOffset to keywordId
    cowvector<unsigned> *keywordIds;
    bool commited_WriteView;

    ForwardIndex *forwardIndex; //Not serialised, must be assigned after every load and save.

    // Index Build time
    vector<unsigned> invertedListSizeDirectory;

    // Used by InvertedIndex::merge()
    set<unsigned> invertedListSetToMerge;

    // Merge is required if the list is not empty
    bool mergeRequired() const  { return !(invertedListSetToMerge.empty()); }

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & invertedIndexVector;
        ar & keywordIds;
        ar & commited_WriteView;
    }
};

}}


#endif //__INVERTEDINDEX_H__