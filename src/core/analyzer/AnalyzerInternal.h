/*
 * AnalyzerInternal.h
 *
 *  Created on: 2013-5-17
 *
 */

#ifndef __CORE_ANALYZER__ANALYZER_INTERNAL_H__
#define __CORE_ANALYZER__ANALYZER_INTERNAL_H__

#include <vector>
#include <map>
#include <fstream>
#include <instantsearch/Analyzer.h>
#include <boost/regex.hpp>
#include "TokenStream.h"
#include "util/Logger.h"

using std::vector;
using std::map;
using std::string;
using std::stringstream;
using srch2::util::Logger;

namespace srch2 {
namespace instantsearch {

class Record;

struct TokenAttributeHits {
	/** Each entry has position information as follows:
	 *  Attribute -> First 8bits -> Attribute in which the token hit occurred
	 *  Hits -> Last 24 bits -> Position within the attribute where the token hit occurred.
	 *  The positions start from 1, this is because the positions in PositionIndex are ZERO terminated.
	 *
	 *  The maximum number of allowed Attributes is checked by the following assert
	 *  ASSERT( attribute <  0xff);
	 *
	 *  i.e. 255
	 *
	 *  The maximum number of the positionHit is checked by the following assert
	 *  ASSERT( position <  0xffffff);
	 *
	 * i.e. 4 294 967 295
	 *
	 */
	vector<unsigned> attributeList;
};

class AnalyzerInternal {
public:

	AnalyzerInternal(const AnalyzerInternal &analyzerInternal);

	AnalyzerInternal(const StemmerNormalizerFlagType &stemmerFlag,
			const std::string &recordAllowedSpecialCharacters,
			const std::string &stemmerFilePath = "",
			const std::string &stopWordFilePath = "",
			const std::string &synonymFilePath = "",
			const SynonymKeepOriginFlag &synonymKeepOriginFlag = SYNONYM_KEEP_ORIGIN);

	void loadData(const std::string &s) const;


	virtual TokenStream * createOperatorFlow() = 0;
	virtual ~AnalyzerInternal() {
	}


	void prepareRegexExpression() {
		//allow all characters
		string regexString = "[^A-Za-z0-9 "
				+ CharSet::getRecordAllowedSpecialCharacters() + "\x80-\xFF"
				+ "]";
		try {
			disallowedCharactersRegex = boost::regex(regexString);
		} catch (boost::regex_error& e) {

            Logger::error("%s is not a valid regular expression. Using default: [^A-Za-z0-9 ]", regexString.c_str());
			disallowedCharactersRegex = boost::regex("[^A-Za-z0-9 ]");
		}

		multipleSpaceRegex = boost::regex(" +");
		headTailSpaceRegex = boost::regex("^[ \t]+|[ \t]+$");
	}

	const string cleanString(const std::string &inputString) const {
		// example: " ab$(cd " -> " ab  cd "
		const std::string string1 = boost::regex_replace(inputString,
				disallowedCharactersRegex, DEFAULT_DELIMITER_STRING);

		// example: " ab  cd " -> " ab cd "
		const std::string string2 = boost::regex_replace(string1,
				multipleSpaceRegex, DEFAULT_DELIMITER_STRING);

		// example: " ab cd " -> "ab cd"
		const std::string string3 = boost::regex_replace(string2,
				headTailSpaceRegex, "");

		return string3;
	}

	static void load(AnalyzerInternal &analyzer,
			boost::archive::binary_iarchive &ia) {
		ia >> analyzer;
	}
	;
	const AnalyzerType& getAnalyzerType() const {
		return analyzerType;
	}

	static void save(const AnalyzerInternal &analyzer,
			boost::archive::binary_oarchive &oa) {
		oa << analyzer;
	}
	;

	/**
	 * Function to tokenize a given record.
	 * @param[in] record
	 * @param[in, out] tokenAttributeHitsMap
	 */
	void tokenizeRecord(const Record *record,
			map<string, TokenAttributeHits> &tokenAttributeHitsMap) const;

protected:
	boost::shared_ptr<TokenStreamContainer> tokenStreamContainer;

	TokenStream* tokenStream;
	AnalyzerType analyzerType;
	StemmerNormalizerFlagType stemmerType; // This flag shows that we want to stem or not.
	string recordAllowedSpecialCharacters;
	string stopWordFilePath;
	string synonymFilePath;
	string stemmerFilePath;
	SynonymKeepOriginFlag synonymKeepOriginFlag;

	boost::regex disallowedCharactersRegex;
	boost::regex multipleSpaceRegex;
	boost::regex headTailSpaceRegex;

	friend class boost::serialization::access;
	friend class Analyzer;

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & recordAllowedSpecialCharacters;
		ar & analyzerType;
		ar & stopWordFilePath;
		ar & synonymFilePath;
		ar & synonymKeepOriginFlag;
		ar & stemmerFilePath;
	}

};

}
}
#endif /* __CORE_ANALYZER__ANALYZER_INTERNAL_H__ */
