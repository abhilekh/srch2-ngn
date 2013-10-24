//$Id: JSONRecordParser.h 3456 2013-06-14 02:11:13Z jiaying $

#ifndef _DAEMONDATASOURCE_H_
#define _DAEMONDATASOURCE_H_

#include "json/json.h"
#include <instantsearch/Schema.h>
#include <instantsearch/Analyzer.h>
#include <instantsearch/Record.h>
#include <instantsearch/Indexer.h>

#include "ConfigManager.h"

namespace srch2is = srch2::instantsearch;

namespace srch2
{
namespace httpwrapper
{


class JSONRecordParser
{
 public:
  static bool populateRecordFromJSON( const std::string &inputLine, const ConfigManager *indexDataContainerConf,
				      srch2is::Record *record, std::stringstream &error);
  static bool _JSONValueObjectToRecord(srch2is::Record *record, const std::string &inputLine, const Json::Value &root,
				       const ConfigManager *indexDataContainerConf, std::stringstream &error);
  static srch2is::Schema* createAndPopulateSchema( const ConfigManager *indexDataContainerConf);

 private:
  static void getJsonValueString(const Json::Value &jsonValue, const std::string &key, std::string &stringValue, const string &configName);
  static void getJsonValueDateAndTime(const Json::Value &jsonValue, const std::string &key, std::string &stringValue, const string &configName, bool isMultiValued);
  static void getJsonValueDouble(const Json::Value &jsonValue, const std::string &key, double &doubleValue, const string& configName);
};

class DaemonDataSource
{
	public:
		static void createNewIndexFromFile(srch2is::Indexer *indexer, const ConfigManager *indexDataContainerConf);
};


}}

#endif // _DAEMONDATASOURCE_H_
