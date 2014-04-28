#include "InternalMessageBroker.h"


namespace srch2is = srch2::instantsearch;
using namespace std;

namespace srch2 {
namespace httpwrapper {



void InternalMessageBroker::processInternalMessage(Message * message){
	if(message == NULL){
		return;
	}

	Srch2Server* server = getShardIndex(message->shard);
	if(server == NULL){
		//TODO : what if message shardID is not present in the map?
		// example : message is late and shard is not present anymore ....
		return;
	}

	//1. Deserialize message and get command input 	object
	//2. Call the appropriate internal DP function and get the response object
	//3. Serialize response object into a message
	//4. give the new message out
	switch (message->type) {
		case SearchCommandMessageType: // -> for LogicalPlan object
		{
			//1. Deserialize message and get command input object
			SerializableSearchCommandInput & searchInput = SerializableSearchCommandInput::deserialize(message->buffer);
			//2. Call the appropriate internal DP function and get the response object
			SerializableSearchResults & searchResults = routingManager->getDpInternal()->internalSearchCommand(server, &searchInput);
			//3. Serialize response object into a message
			void * seralizedSearchResults = searchResults.serialize(routingManager->getAllocator());
			//4. give the new message out
			//TODO : sendReply(seralizedSearchResults)
			break;
		}
		case InsertUpdateCommandMessageType: // -> for Record object (used for insert and update)
		{
			//1. Deserialize message and get command input object
			SerializableInsertUpdateCommandInput & insertUpdateInput = SerializableInsertUpdateCommandInput::deserialize(message->buffer);
			//2. Call the appropriate internal DP function and get the response object
			SerializableCommandStatus & insertUpdateResults = routingManager->getDpInternal()->internalInsertUpdateCommand(server, &insertUpdateInput);
			//3. Serialize response object into a message
			void * seralizedInsertUpdateResults = insertUpdateResults.serialize(routingManager->getAllocator());
			//4. give the new message out
			//TODO : sendReply(seralizedInsertUpdateResults)
		    break;
		}
		case DeleteCommandMessageType: // -> for DeleteCommandInput object (used for delete)
		{
			//1. Deserialize message and get command input object
			SerializableDeleteCommandInput & deleteInput = SerializableDeleteCommandInput::deserialize(message->buffer);
			//2. Call the appropriate internal DP function and get the response object
			SerializableCommandStatus & deleteResults = routingManager->getDpInternal()->internalDeleteCommand(server, &deleteInput);
			//3. Serialize response object into a message
			void * seralizedDeleteResults = deleteResults.serialize(routingManager->getAllocator());
			//4. give the new message out
			//TODO : sendReply(seralizedDeleteResults)
		    break;
		}
		case SerializeCommandMessageType: // -> for SerializeCommandInput object (used for serializing index and records)
		{
			//1. Deserialize message and get command input object
			SerializableSerializeCommandInput & serializeInput = SerializableSerializeCommandInput::deserialize(message->buffer);
			//2. Call the appropriate internal DP function and get the response object
			SerializableCommandStatus & serializeResults = routingManager->getDpInternal()->internalSerializeCommand(server, &serializeInput);
			//3. Serialize response object into a message
			void * seralizedSerializeCommandResults = serializeResults.serialize(routingManager->getAllocator());
			//4. give the new message out
			//TODO : sendReply(seralizedSerializeCommandResults)
		 	break;
		}
		case GetInfoCommandMessageType: // -> for GetInfoCommandInput object (used for getInfo)
		{
			//1. Deserialize message and get command input object
			SerializableGetInfoCommandInput & getInfoInput = SerializableGetInfoCommandInput::deserialize(message->buffer);
			//2. Call the appropriate internal DP function and get the response object
			//TODO : where should we get version info from ?
			SerializableGetInfoResults & getInfoResults = routingManager->getDpInternal()->internalGetInfoCommand(server, "version info" , &getInfoInput);
			//3. Serialize response object into a message
			void * seralizedGetInfoResults = getInfoResults.serialize(routingManager->getAllocator());
			//4. give the new message out
			//TODO : sendReply(seralizedGetInfoResults)
			break;
		}
		case CommitCommandMessageType: // -> for CommitCommandInput object
		{
			//1. Deserialize message and get command input object
			SerializableCommitCommandInput & commitInput = SerializableCommitCommandInput::deserialize(message->buffer);
			//2. Call the appropriate internal DP function and get the response object
			//TODO : where should we get version info from ?
			SerializableCommandStatus & commitResults = routingManager->getDpInternal()->internalCommitCommand(server , &commitInput);
			//3. Serialize response object into a message
			void * seralizedCommitResults = commitResults.serialize(routingManager->getAllocator());
			//4. give the new message out
			//TODO : sendReply(seralizedCommitResults)
			break;
		}
		case ResetLogCommandMessageType: // -> for ResetLogCommandInput (used for resetting log)
		{
			//1. Deserialize message and get command input object
			SerializableResetLogCommandInput & resetLogInput = SerializableResetLogCommandInput::deserialize(message->buffer);
			//2. Call the appropriate internal DP function and get the response object
			//TODO : where should we get version info from ?
			SerializableCommandStatus & resetLogResults = routingManager->getDpInternal()->internalResetLogCommand(server , &resetLogInput);
			//3. Serialize response object into a message
			void * seralizedCommitResults = resetLogResults.serialize(routingManager->getAllocator());
			//4. give the new message out
			//TODO : sendReply(seralizedCommitResults)
			break;
		}
		default:
		{
			//TODO : what should we do here ?
			break;
		}
	}
}

Srch2Server * InternalMessageBroker::getShardIndex(ShardId & shardId){
	//get shardId from message to use map to get Indexer object
	std::map<ShardId, Srch2Server*>::iterator indexerItr = routingManager->getShardToIndexMap().find(shardId);
	if(indexerItr == routingManager->getShardToIndexMap().end()){
		return NULL;
	}

	return indexerItr->second;
}

}
}
