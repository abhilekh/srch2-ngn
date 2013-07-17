
// $Id: AnalyzerInternalold.cpp 3375 2013-05-24 06:54:28Z huaijie $

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

#include "AnalyzerInternalold.h"
#include <instantsearch/Record.h>
#include <instantsearch/Schema.h>
#include <util/Assert.h>
#include "Normalizer.h"
#include "analyzer/StemmerInternal.h"
#include "AnalyzerInternal.h"

#include <boost/algorithm/string.hpp>

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <sstream>

//#include <iostream>

using namespace std;

namespace srch2
{
namespace instantsearch
{
bool isEmpty(const string &inString);
unsigned setAttributePositionBitVector(unsigned attribute, unsigned position);

AnalyzerInternalold::AnalyzerInternalold()
{
    this->stemmer = NULL;
    this->normalizer = NULL;
    this->analyzers = new StandardAnalyzer();
    this->tokenOperator = analyzers->createOperatorFlow();
}

AnalyzerInternalold::AnalyzerInternalold(const StemmerNormalizerFlagType &stemmerFlag,const std::string &recordAllowedSpecialCharacters)
{
    this->recordAllowedSpecialCharacters = recordAllowedSpecialCharacters;
    this->prepareRegexExpression();
    this->stemmerFlag = stemmerFlag;
    this->stemmer = NULL;
    this->normalizer = NULL;
    this->analyzers = new StandardAnalyzer();
    this->tokenOperator = analyzers->createOperatorFlow();
}

//Copy Constructor
AnalyzerInternalold::AnalyzerInternalold(const AnalyzerInternalold &analyzerInternal, const std::string &indexDirectory)
{
    this->stemmerFlag = analyzerInternal.stemmerFlag;
    this->stemmer = new Stemmer(this->getStemmerNormalizerFlagType(), indexDirectory);
    this->normalizer = new Normalizer(this->getStemmerNormalizerFlagType(), indexDirectory);
    this->analyzers = new StandardAnalyzer();
    this->tokenOperator = analyzers->createOperatorFlow();
    this->recordAllowedSpecialCharacters = analyzerInternal.getRecordAllowedSpecialCharacters();
    this->prepareRegexExpression();
}

AnalyzerInternalold::~AnalyzerInternalold()
{
    if (this->stemmer != NULL)
        delete this->stemmer;

    if (this->normalizer != NULL)
        delete this->normalizer;

    if (this->analyzers != NULL)
    {
    	delete this->analyzers;
    	delete this->tokenOperator;
    }
}

/*
AnalyzerInternal::AnalyzerInternal(const string& stopWordFile, const string& delimiters)
{
    //TODO V1
}

AnalyzerInternal::AnalyzerInternal(const vector<string> &stopWords, const string& delimiters)
{
    //TODO V1
}
 */


/*addDelimitersToString()
 *
 * Description: add a space between adjacent characters if one of them
 * has a unicode >= CHARTYPE_FUZZY_UPPERBOUND. Then later we can use the space as a delimiter
 * to tokenize the string into words
 *
 * Example:
 * inString=abc北京xyz 上海
 * outString=abc 北 京 xyz 上 海
 *
 * Internally we first convert the string to a vector of CharTypes.  We then
 * go through this vector to add space at the right places.  Finally we
 * conver the new vector to the output string.
 *
 * TODO: we need to change this function after we add a Chinese tokenizer.
*/
void addDelimitersToString(const string &inString, string &outString)
{
	vector<CharType> charTypeVector;
	utf8StringToCharTypeVector(inString, charTypeVector);

	vector<CharType> newCharTypeVector;

	for(unsigned int i = 0; i < charTypeVector.size(); i++)
	{
		const CharType &chartype = charTypeVector[i];

		// Conditions to add a delimiter: either the current character or the previous character
		// is >= upper bound
		if (chartype >= CHARTYPE_FUZZY_UPPERBOUND ||
			(i > 0 && charTypeVector[i-1] >= CHARTYPE_FUZZY_UPPERBOUND))
			// and the previous inserted character is not delimter
			if (newCharTypeVector.size() > 0 && newCharTypeVector.back() != DEFAULT_DELIMITER)
				// and this chartype is not a delmiter
				if (chartype != DEFAULT_DELIMITER)
				newCharTypeVector.push_back(DEFAULT_DELIMITER);

		newCharTypeVector.push_back(chartype);
	}

	// convert the CharType vector to the output string
	charTypeVectorToUtf8String(newCharTypeVector, outString);
}

const string AnalyzerInternalold::cleanString(const std::string &inputString) const
{
	// example: " ab$(cd " -> " ab  cd "
	const std::string string1 = boost::regex_replace(inputString, disallowedCharactersRegex, DEFAULT_DELIMITER_STRING);

	// example: " ab  cd " -> " ab cd "
	const std::string string2 = boost::regex_replace(string1, multipleSpaceRegex, DEFAULT_DELIMITER_STRING);

	// example: " ab cd " -> "ab cd"
	const std::string string3 = boost::regex_replace(string2, headTailSpaceRegex, "");

	return string3;
}


/**
 * Function to tokenize a given record.
 * @param[in] record
 * @param[in, out] tokenAttributeHitsMap
 *
 * Pseudocode:
 * 1. map<string, TokenAttributeHits > tokenAttributeHitsMap of a record is a map from unique tokens to their corresponding attributeHits information(Refer Struct TokenAttributeHits )
 * 2. First, we iterate over the record attributes. So, for each recordAttribute
 *      1. Boost::split is used to get the tokens from attributeValue.
 *      2. We iterate over these tokens and fill the map
 */
void AnalyzerInternalold::tokenizeRecord(const Record *record, map<string, TokenAttributeHits > &tokenAttributeHitsMap ) const
{
    /// Reference: Tokenizing String in C++ http://stackoverflow.com/questions/236129/how-to-split-a-string

    tokenAttributeHitsMap.clear();
    const Schema *schema = record->getSchema();
    // token string to vector<CharType>
    vector<string> tokens;
    vector<string>::iterator tokenIterator;
    unsigned positionIterator;
    unsigned  size;
    for(unsigned attributeIterator = 0; attributeIterator < schema->getNumberOfSearchableAttributes(); attributeIterator++)
    {
        string *attributeValue = record->getSearchableAttributeValue(attributeIterator);
        if (attributeValue != NULL)
        {
            tokens.clear();
            this->analyzers->loadData(*attributeValue);
            string currentToken = "";
			while(tokenOperator->incrementToken())//process the token one by one
			{
				vector<CharType> charVector;
				tokenOperator->getCurrentToken(charVector);
				charTypeVectorToUtf8String(charVector, currentToken);
				tokens.push_back(currentToken);
				//cout<<currentToken<<endl;
			}
            size = tokens.size();
        /*    cout << "Before stemming "  <<  tokens.size() << endl;
            for(tokenIterator = tokens.begin();tokenIterator!=tokens.begin() + size;++tokenIterator)
            {
                cout << "tokens before stemming:  "<<  *tokenIterator << endl;
            }
        */
            /*
             * 1. GET THE SIZE OF THE TOKEN LIST
             * 2. CONVERT THE TOKENS IN THE TOKEN LIST IN THE LOWER CASE
             * 3. ADD ALL THE STEMMED/NORMALIZED TOKENS AT THE END OF EXISTING TOKEN LIST
             * 4. SET THE POSITION FOR THE STEMMED/NORMALIZED TOKENS AS 0FFFFFF-1
             *
             */

            //ONLY NORMALIZER
           /* if(this->stemNormType == srch2::instantsearch::ONLY_NORMALIZER)
            {
                for(tokenIterator = tokens.begin();tokenIterator!=tokens.begin() + size;++tokenIterator)
                {
                    std::transform(tokenIterator->begin(), tokenIterator->end(), tokenIterator->begin(), ::tolower);
                }
                this->normalizer->normalize(tokens);
                //cout << "After stemming "  <<  tokens.size() << endl;

                for(tokenIterator = tokens.begin();tokenIterator!=tokens.end();++tokenIterator)
                {
                    cout << "tokens after stemming:  "<<  *tokenIterator << endl;
                }

                ///Positions start from one, because the PositionIndex is ZERO terminated.
                positionIterator = 1;
                for(unsigned i =0; i < size; ++i, ++positionIterator)
                {
                    if(tokens[i].size())
                    {
                        tokenAttributeHitsMap[tokens[i]].attributeList.push_back(setAttributePositionBitVector(attributeIterator, positionIterator) );
                    }
                }
                /// Set the position of the Normalized token as 0xffffff-1
                for(unsigned i=size; i<tokens.size();++i)
                {
                    unsigned val =  setAttributePositionBitVector(attributeIterator, 0xffffff-1);
                    if(tokens[i].size())
                    {
                        tokenAttributeHitsMap[tokens[i]].attributeList.push_back(val);
                    }
                }
            }

            //COMPLETE STEMMER (PORTER STEMMER + NORMALIZER)
            else if(this->stemNormType == srch2::instantsearch::STEMMER_NORMALIZER)
            {
                for(tokenIterator = tokens.begin();tokenIterator!=tokens.begin() + size;++tokenIterator)
                {
                    std::transform(tokenIterator->begin(), tokenIterator->end(), tokenIterator->begin(), ::tolower);
                }

                for(unsigned i = 0; i < size; ++i)
                {
                    string token = tokens[i];
                    //cout<<"Printing the tokens before stemming:  "<<token<<endl;
                    std::string stemmedToken = this->stemmer->stemToken(token);
                    if(strcmp(token.c_str(), stemmedToken.c_str())!=0)
                    {
                        //cout << "Printing the tokens after stemming:  "<< stemmedToken << endl;
                        tokens.push_back(stemmedToken);
                    }
                }

                ///Call Normalizer with tokens
                this->normalizer->normalize(tokens);

                ///Positions start from one, because the PositionIndex is ZERO terminated.
                positionIterator = 1;
                for(unsigned i =0; i < size; ++i, ++positionIterator)
                {
                    if(tokens[i].size())
                    {
                        tokenAttributeHitsMap[tokens[i]].attributeList.push_back(setAttributePositionBitVector(attributeIterator, positionIterator) );
                    }
                }
                /// Set the position of the stemmed/normalized token as 0xffffff-1
                for(unsigned i=size; i<tokens.size();++i)
                {
                    unsigned val =  setAttributePositionBitVector(attributeIterator, 0xffffff-1);
                    if(tokens[i].size())
                    {
                        tokenAttributeHitsMap[tokens[i]].attributeList.push_back(val);
                    }
                }
            }*/
            // NO STEMMER/NORMALIZER
           // else
         //   {
                positionIterator = 1;
                for(tokenIterator = tokens.begin(); tokenIterator != tokens.end(); ++tokenIterator, ++positionIterator )
                {

                    ///TODO OPT: Remove this comparison and make sure, split returns no empty strings.
                    if (tokenIterator->size())
                    {
                        //Convert to lowercase
                        std::transform(tokenIterator->begin(), tokenIterator->end(), tokenIterator->begin(), ::tolower);

                        tokenAttributeHitsMap[*tokenIterator].attributeList.push_back(setAttributePositionBitVector(attributeIterator, positionIterator) );
                    }
                }
           // }
        }
    }
}

//token utf-8 string to vector<vector<CharType> >
void AnalyzerInternalold::tokenizeQuery(const string &queryString, vector<string> &queryKeywords, const char &delimiterCharacter) const
{

    queryKeywords.clear();
	this->analyzers->loadData(queryString);
	string currentToken = "";
	while(tokenOperator->incrementToken())//process the token one by one
	{
		vector<CharType> charVector;
		tokenOperator->getCurrentToken(charVector);
		charTypeVectorToUtf8String(charVector, currentToken);
		queryKeywords.push_back(currentToken);
		//cout<<currentToken<<endl;
	}

    if (queryKeywords.size() == 1 && isEmpty(queryKeywords[0]))
            queryKeywords.clear();
}

void AnalyzerInternalold::tokenizeQueryWithFilter(const string &queryString, vector<string> &queryKeywords, const char &delimiterCharacter,
                                               const char &filterDelimiterCharacter, const char &fieldsAndCharacter, const char &fieldsOrCharacter,
                                               const map<string, pair<unsigned, unsigned> > *searchableAttributesTriple, vector<unsigned> &filters) const
{
    stringstream charToString;
    string delimiter;
    string filterDelimiter;
    string fieldDelimiter;
    charToString << delimiterCharacter;
    charToString >> delimiter;
    charToString.clear();
    charToString << filterDelimiterCharacter;
    charToString >> filterDelimiter;
    charToString.clear();
    charToString << fieldsAndCharacter;
    charToString << fieldsOrCharacter;
    charToString >> fieldDelimiter;

    string query = queryString;
    queryKeywords.clear();
    filters.clear();
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    vector<string> parts;
    replace_if(query.begin(), query.end(), boost::is_any_of(delimiter), DEFAULT_DELIMITER);

    boost::split(parts, query, boost::is_any_of(" "));

    bool malformed = false;
    for (unsigned i=0; i<parts.size(); i++)
    {
        replace_if(parts[i].begin(), parts[i].end(), boost::is_any_of(filterDelimiter), DEFAULT_DELIMITER);
        vector<string> one_pair;
        boost::split(one_pair, parts[i], boost::is_any_of(" "));

        if (one_pair.size() > 2 || one_pair.size() == 0)
        {
            malformed = true;
            break;
        }

        const string cleanString = this->cleanString(one_pair[0]);
        queryKeywords.push_back(cleanString);

        if (one_pair.size() == 1) // have no filter information
        {
            filters.push_back(0x7fffffff); // can appear in any field, the top bit is reserved for AND/OR relationship.
            continue;
        }

        vector<string> fields;

        bool AND = false;
        bool OR = false;
        if (one_pair[1].find(fieldsAndCharacter) != string::npos)
            AND = true;
        if (one_pair[1].find(fieldsOrCharacter) != string::npos)
            OR = true;
        if (AND && OR)
        {
            malformed = true;
            break;
        }

        boost::split(fields, one_pair[1], boost::is_any_of(fieldDelimiter));
        unsigned filter = 0;
        for (unsigned j = 0; j<fields.size(); j++)
        {
            map<string, pair<unsigned, unsigned> >::const_iterator iter = searchableAttributesTriple->find(fields[j]);
            if (iter == searchableAttributesTriple->end())
            {
                malformed = true;
                break;
            }

            unsigned bit = 1;
            ASSERT(iter->second.first < 31); // the left most bit is reserved for indicating the AND/OR relation between fields
            bit <<= iter->second.first;
            filter |= bit;
        }
        if (AND)
            filter |= 0x80000000;

        if (malformed)
            break;
        else
            filters.push_back(filter);
    }

    if ( malformed
            || (queryKeywords.size() == 1 && isEmpty(queryKeywords[0])))
        queryKeywords.clear();

}

}}
