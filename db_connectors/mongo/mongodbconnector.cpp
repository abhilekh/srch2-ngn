/*
 * mongodbconnector.cpp
 *
 *  Created on: Jun 9, 2014
 *      Author: liusrch2
 */
#include "mongodbconnector.h"
#include <iostream>
#include <unistd.h>
#include "mongo/client/dbclient.h"
#include <iomanip>
#include <fstream>
#include <boost/filesystem.hpp>

using namespace std;

MongoDBConnector::MongoDBConnector() {
    serverHandle = NULL;
    oplogConnection = NULL;
    mongoConnector = NULL;
}

//Init the connector, call connect
int MongoDBConnector::init(ServerInterface *serverHandle) {
    this->serverHandle = serverHandle;

    // For MongoDB, the primary key should always be "_id".
    std::string uniqueKey;
    this->serverHandle->configLookUp("uniqueKey", uniqueKey);
    if (uniqueKey.compare("_id") != 0) {
        printf("MOGNOLISTENER: The PrimaryKey in the config file for the "
                "MongoDB adapter should always be \"_id\", not %s",
                uniqueKey.c_str());
        return -1;
    }

    if (!checkConfigValidity() || !connectToDB()) {
        return -1;
    }
    return 0;
}

//Check config validity. e.g. if contains port, dbname, etc.
bool MongoDBConnector::checkConfigValidity() {
    std::string host, port, db, uniqueKey, collection;
    this->serverHandle->configLookUp("host", host);
    this->serverHandle->configLookUp("port", port);
    this->serverHandle->configLookUp("db", db);
    this->serverHandle->configLookUp("uniqueKey", uniqueKey);
    this->serverHandle->configLookUp("collection", collection);

    bool ret = (host.size() != 0) && (port.size() != 0) && (db.size() != 0)
            && (uniqueKey.size() != 0) && (collection.size() != 0);
    if (!ret) {
        printf("MOGNOLISTENER: database host, port, db, collection, uniquekey must be set.\n");
        return false;
    }

    int value = atoi(port.c_str());
    if (value <= 0 || value > USHRT_MAX) {
        printf("MOGNOLISTENER: database port must be between 1 and %d", USHRT_MAX);
        return false;
    }

    return true;
}

//Connect to the mongodb
bool MongoDBConnector::connectToDB() {
    string mongoNamespace = "local.oplog.rs";

    string host, port, listenerWaitTimeStr, maxRetryOnFailureStr;
    this->serverHandle->configLookUp("host", host);
    this->serverHandle->configLookUp("port", port);
    this->serverHandle->configLookUp("listenerWaitTime", listenerWaitTimeStr);
    this->serverHandle->configLookUp("maxRetryOnFailure", maxRetryOnFailureStr);

    int listenerWaitTime = 1;
    if (listenerWaitTimeStr.size() != 0) {
        listenerWaitTime = atoi(listenerWaitTimeStr.c_str());
    }

    int maxRetryOnFailure = 3;
    if (maxRetryOnFailureStr.size() != 0) {
        maxRetryOnFailure = atoi(maxRetryOnFailureStr.c_str());
    }

    for (unsigned retryCount = -1; retryCount != maxRetryOnFailure;
            retryCount++) {

        try {
            string hostAndport = host;
            if (port.size()) {
                hostAndport.append(":").append(port);
            }
            mongoConnector = new mongo::ScopedDbConnection(hostAndport);
            oplogConnection = &mongoConnector->conn();

            // first check whether the replication is
            // enabled and the host is primary of the replica set
            if (!oplogConnection->exists(mongoNamespace)) {
                printf("MOGNOLISTENER: oplog does not exist on host = %s \n",
                        host.c_str());
                printf("MOGNOLISTENER: either replication is not enabled on"
                        " the host instance or the host is not a primary "
                        "member of replica set \n");
                printf("MOGNOLISTENER: exiting ... \n");
                return false;
            }

            return true;
        } catch (const mongo::DBException &e) {
            printf("MOGNOLISTENER: MongoDb Exception %s \n", e.what());
        } catch (const exception& ex) {
            printf("MOGNOLISTENER: Unknown exception %s \n", ex.what());
        }
        // sleep...do not hog the CPU
        sleep(listenerWaitTime);
        printf("MONGOLISTENER: trying again ...");
    }
    // if all retries failed then exit the thread
    printf("MONGOLISTENER: exiting...\n");
    return false;
}

//Load the table records and insert into the engine
int MongoDBConnector::createNewIndexes() {
    string mongoNamespace = "local.oplog.rs";
    string dbname, collection;
    this->serverHandle->configLookUp("db", dbname);
    this->serverHandle->configLookUp("collection", collection);
    string filterNamespace = dbname + "." + collection;
    try {
        unsigned collectionCount = oplogConnection->count(filterNamespace);
        // We fetch data from mongo db only
        // if there are some records to be processed
        unsigned indexedRecordsCount = 0;
        if (collectionCount > 0) {
            // query the mongo database for all the objects in collection.
            // mongo::BSONObj() means get all records from mongo db
            auto_ptr<mongo::DBClientCursor> cursor = oplogConnection->query(
                    filterNamespace, mongo::BSONObj());
            // get data from cursor
            while (cursor->more()) {
                mongo::BSONObj obj = cursor->next();
                // parse BSON object returned by cursor
                // and fill in record object
                string recNS = obj.getStringField("ns");

                string jsonRecord = obj.jsonString();
                this->serverHandle->insertRecord(jsonRecord);

                ++indexedRecordsCount;
                if (indexedRecordsCount && (indexedRecordsCount % 1000) == 0)
                    printf("MOGNOLISTENER: Indexed %d records so far ...",
                            indexedRecordsCount);
            }
            printf("MOGNOLISTENER: Total indexed %d / %d records. \n", indexedRecordsCount,
                    collectionCount);
            //Save the time right after create new indexes.
            setLastAccessedLogRecordTime(time(NULL));
            this->serverHandle->saveChanges();

        } else {
            printf("MOGNOLISTENER: No data found in the collection %s",
                    filterNamespace.c_str());
        }
    } catch (const mongo::DBException &e) {
        printf("MOGNOLISTENER: MongoDb Exception %s", e.what());
        return -1;
    } catch (const exception& ex) {
        printf("MOGNOLISTENER: Unknown exception %s", ex.what());
        return -1;
    }

    return 0;
}

//Load the last time last oplog record accessed
bool MongoDBConnector::getLastAccessedLogRecordTime(time_t& t) {
    std::string dataDir, srch2Home;

    this->serverHandle->configLookUp("srch2Home", srch2Home);
    this->serverHandle->configLookUp("dataDir", dataDir);
    std::string path = srch2Home + "/" + dataDir + "/mongodb_data/"
            + "data.bin";

    if (access(path.c_str(), F_OK) == 0) {
        ifstream a_file(path.c_str(), ios::in | ios::binary);
        a_file >> t;
        a_file.close();
        return true;
    } else {
        printf("MONGOLISTENER: Can not find %s. Listener starts failed.\n",
                path.c_str());
        return false;
    }
}

//Save the time last oplog record accessed
void MongoDBConnector::setLastAccessedLogRecordTime(const time_t t) {
    std::string srch2Home;
    std::string dataDir;
    this->serverHandle->configLookUp("srch2Home", srch2Home);
    this->serverHandle->configLookUp("dataDir", dataDir);
    std::string path = srch2Home + "/" + dataDir + "/mongodb_data/";

    if (access(path.c_str(), F_OK) != 0) {
        boost::filesystem::create_directories(path);
    }
    std::string pt = path + "data.bin";
    ofstream a_file(pt.c_str(), ios::trunc | ios::binary);
    a_file << t;
    a_file.flush();
    a_file.close();
}

//Listen to the oplog and do modification to the engine
int MongoDBConnector::runListener() {
    string mongoNamespace = "local.oplog.rs";
    string dbname, collection, listenerWaitTimeStr;
    this->serverHandle->configLookUp("db", dbname);
    this->serverHandle->configLookUp("collection", collection);
    this->serverHandle->configLookUp("listenerWaitTime", listenerWaitTimeStr);
    string filterNamespace = dbname + "." + collection;

    int listenerWaitTime = 1;
    if (listenerWaitTimeStr.size() != 0) {
        listenerWaitTime = atoi(listenerWaitTimeStr.c_str());
    }

    do {
        bool printOnce = true;
        time_t opLogTime = 0;
        time_t threadSpecificCutOffTime = 0;
        if(!getLastAccessedLogRecordTime(threadSpecificCutOffTime)){
            printf("MONGOLISTENER: exiting...\n");
            mongoConnector->done();
            return -1;
        }
        try {
            mongo::BSONElement _lastValue = mongo::BSONObj().firstElement();

            mongo::Query query = mongo::Query().hint(BSON("$natural" << 1));
            while (1) {
                /*
                 * open the tail cursor on the capped collection oplog.rs
                 * the cursor will wait for more data when it reaches at
                 * the end of the collection. For more info please see the
                 * following link.
                 * http://docs.mongodb.org/manual/tutorial/create-tailable-cursor/
                 */
                auto_ptr<mongo::DBClientCursor> tailCursor =
                        oplogConnection->query(mongoNamespace, query, 0, 0, 0,
                                mongo::QueryOption_CursorTailable
                                        | mongo::QueryOption_AwaitData);
                while (1) {
                    if (tailCursor->more()) {
                        mongo::BSONObj obj = tailCursor->next();
                        string recNS = obj.getStringField("ns");
                        if (recNS.compare(filterNamespace) == 0) {
                            mongo::BSONElement timestampElement = obj.getField(
                                    "ts");
                            /*
                             * No timestampElement.timestampTime().toTimeT()
                             * in mongo-cxx-driver-legacy-0.0-26compat-2.6.2
                             * This is a temporarily solution
                             */
                            std::stringstream ss;
                            ss << timestampElement.timestampTime();
                            ss >> opLogTime;
                            opLogTime = opLogTime / 1000;
                            if (opLogTime > threadSpecificCutOffTime) {
                                parseOpLogObject(obj, filterNamespace,
                                        *oplogConnection);
                            }
                        }
                        _lastValue = obj["_id"];
                        printOnce = true;
                    } else {
                        /*
                         *  Cursor is either dead or does not have more records
                         *  store the timestamp of the last record processed,
                         *  so that when the cursor fetches more data we can
                         *  filter out any records processed earlier.
                         *  Alternative is to store current time.
                         */
                        if (threadSpecificCutOffTime != opLogTime) {
                            setLastAccessedLogRecordTime(opLogTime);
                            this->serverHandle->saveChanges();
                        }
                        threadSpecificCutOffTime = opLogTime;
                        if (tailCursor->isDead())
                            break;
                        if (printOnce) {
                            printf("MOGNOLISTENER: waiting for updates ... \n");
                            printOnce = false;
                        }

                        // sleep...do not hog the CPU
                        sleep(listenerWaitTime);
                    }
                }
                query = QUERY(
                        "_id" << mongo::GT << _lastValue).hint(
                        BSON( "$natural" << 1 ) );
            }
        } catch( const mongo::DBException &e ) {
            printf("MOGNOLISTENER: MongoDb Exception %s \n", e.what());
            //Need to save the time stamp when the mongodb crashed
            setLastAccessedLogRecordTime(opLogTime);
        } catch (const exception& ex) {
            printf("MOGNOLISTENER: Unknown exception %s \n", ex.what());
            setLastAccessedLogRecordTime(opLogTime);
        }
        sleep(listenerWaitTime);
    } while (connectToDB());	//Retry connecting to the mongodb

    mongoConnector->done();
    return -1;
}

//Parse the record into json format and do the corresponding operation
void MongoDBConnector::parseOpLogObject(mongo::BSONObj& bobj,
        string filterNamespace, mongo::DBClientBase& oplogConnection) {
    printf("MOGNOLISTENER: Processing %s \n", bobj.jsonString().c_str());
    string operation = bobj.getField("op").valuestrsafe();
    if (operation.size() == 0) {
        printf("MOGNOLISTENER: oplog entry does not have op "
                "field or the value is not set \n");
        operation = "x"; // undefined operation
    }

    string uniqueKey;
    this->serverHandle->configLookUp("uniqueKey", uniqueKey);

    switch (operation[0]) {
    case 'i':
    case 'I': {
        // Parse example data and insert
        mongo::BSONObj bsonData = bobj.getObjectField("o");
        string jsonRecord = bsonData.jsonString();
        this->serverHandle->insertRecord(jsonRecord);
        break;
    }
    case 'd':
    case 'D': {
        // Parse example data , find the pk and delete
        mongo::BSONElement _oElement = bobj.getField("o");
        if (_oElement.type() != mongo::Object) {
            printf("MONGOLISTENER: \"o\" element is "
                    "not an Object type in the delete operation!! ..Cannot update engine \n");
            break;
        }
        mongo::BSONElement pk = _oElement.Obj().getField(uniqueKey.c_str());
        string primaryKeyStringValue;
        if (pk.type() == mongo::jstOID && uniqueKey.compare("_id") == 0) {
            mongo::OID oid = pk.OID();
            primaryKeyStringValue = oid.str();
        } else {
            primaryKeyStringValue = pk.valuestrsafe();
        }

        printf("MOGNOLISTENER: Delete pk = %s  val =  %s \n", uniqueKey.c_str(),
                primaryKeyStringValue.c_str());
        this->serverHandle->deleteRecord(primaryKeyStringValue);

        break;
    }
    case 'u':
    case 'U': {
        // Parse example data , find the pk and update
        mongo::BSONElement _o2Element = bobj.getField("o2");
        if (_o2Element.type() != mongo::Object) {
            printf("MONGOLISTENER: o2 element is not an"
                    " ObjectId type in the update operation!! ..Cannot update engine\n");
            break;
        }
        mongo::BSONObj _internalMongoId = bobj.getField("o2").Obj();
        auto_ptr<mongo::DBClientCursor> cursor = oplogConnection.query(
                filterNamespace, _internalMongoId);
        mongo::BSONObj updateRecord = cursor->next();  // should return only one

        if (string(updateRecord.firstElementFieldName()).compare("$err") == 0) {
            printf("MONGOLISTENER: updated record could"
                    " not be found in db in the update operation!! ..Cannot update engine \n");
            break;
        }

        mongo::BSONElement pk = _o2Element.Obj().getField(uniqueKey.c_str());
        string primaryKeyStringValue;
        if (pk.type() == mongo::jstOID && uniqueKey.compare("_id") == 0) {
            mongo::OID oid = pk.OID();
            primaryKeyStringValue = oid.str();
        } else {
            primaryKeyStringValue = pk.valuestrsafe();
        }

        string jsonRecord = updateRecord.jsonString();
        this->serverHandle->updateRecord(primaryKeyStringValue, jsonRecord);

        break;
    }
    default:
        break;
        printf("MOGNOLISTENER: The mongodb operation (ops='%c') is not"
                " supported by the engine \n", operation[0]);
    }
}

MongoDBConnector::~MongoDBConnector() {
}

/*
 * "create_t()" and "destroy_t(DataConnector*)" is called to create/delete
 * the instance of the connector. A simple example of implementing these
 * two function is here.
 *
 * extern "C" DataConnector* create() {
 *     return new YourDBConnector;
 * }
 *
 * extern "C" void destroy(DataConnector* p) {
 *     delete p;
 * }
 *
 * These two C APIs are used by the srch2-engine to create/delete the instance
 * in the shared library.
 * The engine will call "create()" to get the connector and call
 * "destroy" to delete it.
 */
extern "C" DataConnector* create() {
    return new MongoDBConnector;
}

extern "C" void destroy(DataConnector* p) {
    delete p;
}
