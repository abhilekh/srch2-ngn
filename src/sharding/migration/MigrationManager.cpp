/*
 * MigrationManager.cpp
 *
 *  Created on: Jul 1, 2014
 *      Author: srch2
 */

#include "MigrationManager.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include "core/util/SerializationHelper.h"

#include "util/Logger.h"
#include "util/Assert.h"
#include "server/Srch2Server.h"
#include "discovery/DiscoveryManager.h"
#ifndef __MACH__
#include <sys/sendfile.h>
#endif
#include <sys/mman.h>
#include "sharding/metadata_manager/Cluster.h"

namespace srch2 {
namespace httpwrapper {

const unsigned BLOCK_SIZE =  1400;  // MTU size

// various migration message's body types
struct MigrationInitMsgBody{
	ClusterShardId migratingShardId;
	ClusterShardId destinationShardId;
	unsigned srcOperationId;
	unsigned dstOperationId;
	unsigned srcNodeId;
	unsigned dstNodeId;
	unsigned shardComponentCount;
	void * serialize(void * buffer)const{
		buffer = migratingShardId.serialize(buffer);
		buffer = destinationShardId.serialize(buffer);
		buffer = srch2::util::serializeFixedTypes(srcOperationId, buffer);
		buffer = srch2::util::serializeFixedTypes(dstOperationId, buffer);
		buffer = srch2::util::serializeFixedTypes(srcNodeId, buffer);
		buffer = srch2::util::serializeFixedTypes(dstNodeId, buffer);
		buffer = srch2::util::serializeFixedTypes(shardComponentCount, buffer);
		return buffer;
	}
	unsigned getNumberOfBytes() const{
		unsigned numberOfBytes = 0;
		numberOfBytes += migratingShardId.getNumberOfBytes();
		numberOfBytes += destinationShardId.getNumberOfBytes();
		numberOfBytes += sizeof(unsigned);
		numberOfBytes += sizeof(unsigned);
		numberOfBytes += sizeof(unsigned);
		numberOfBytes += sizeof(unsigned);
		numberOfBytes += sizeof(unsigned);
		return numberOfBytes;
	}
	void * deserialize(void * buffer){
		buffer = migratingShardId.deserialize(buffer);
		buffer = destinationShardId.deserialize(buffer);
		buffer = srch2::util::deserializeFixedTypes(buffer, srcOperationId);
		buffer = srch2::util::deserializeFixedTypes(buffer, dstOperationId);
		buffer = srch2::util::deserializeFixedTypes(buffer, srcNodeId);
		buffer = srch2::util::deserializeFixedTypes(buffer, dstNodeId);
		buffer = srch2::util::deserializeFixedTypes(buffer, shardComponentCount);
		return buffer;
	}
};

struct MigrationInitAckMsgBody{
	ClusterShardId shardId;
	unsigned ipAddress;
	short portnumber;
	void * serialize(void * buffer)const{
		buffer = shardId.serialize(buffer);
		buffer = srch2::util::serializeFixedTypes(ipAddress, buffer);
		buffer = srch2::util::serializeFixedTypes(portnumber, buffer);
		return buffer;
	}
	unsigned getNumberOfBytes() const{
		unsigned numberOfBytes = 0;
		numberOfBytes += shardId.getNumberOfBytes();
		numberOfBytes += sizeof(unsigned);
		numberOfBytes += sizeof(short);
		return numberOfBytes;
	}
	void * deserialize(void * buffer){
		buffer = shardId.deserialize(buffer);
		buffer = srch2::util::deserializeFixedTypes(buffer, ipAddress);
		buffer = srch2::util::deserializeFixedTypes(buffer, portnumber);
		return buffer;
	}
};

struct ShardComponentInfoMsgBody {
	ClusterShardId shardId;
	unsigned componentSize;
	string componentName;
	void * serialize(void * buffer)const{
		buffer = shardId.serialize(buffer);
		buffer = srch2::util::serializeFixedTypes(componentSize, buffer);
		buffer = srch2::util::serializeString(componentName, buffer);
		return buffer;
	}
	unsigned getNumberOfBytes() const{
		unsigned numberOfBytes = 0;
		numberOfBytes += shardId.getNumberOfBytes();
		numberOfBytes += sizeof(unsigned);
		numberOfBytes += sizeof(unsigned) + componentName.size();
		return numberOfBytes;
	}
	void * deserialize(void * buffer){
		buffer = shardId.deserialize(buffer);
		buffer = srch2::util::deserializeFixedTypes(buffer, componentSize);
		buffer = srch2::util::deserializeString(buffer, componentName);
		return buffer;
	}
};

struct ShardComponentInfoAckMsgBody{
	ClusterShardId shardId;
	void * serialize(void * buffer)const{
		buffer = shardId.serialize(buffer);
		return buffer;
	}
	unsigned getNumberOfBytes() const{
		unsigned numberOfBytes = 0;
		numberOfBytes += shardId.getNumberOfBytes();
		return numberOfBytes;
	}
	void * deserialize(void * buffer){
		buffer = shardId.deserialize(buffer);
		return buffer;
	}
};

struct MigrationData {
	ClusterShardId shardId;
	unsigned sequenceNumber;
	char data[BLOCK_SIZE];
	void * serialize(void * buffer)const{
		buffer = shardId.serialize(buffer);
		buffer = srch2::util::serializeFixedTypes(sequenceNumber, buffer);
		memcpy(buffer, data, BLOCK_SIZE);
		return buffer;
	}
	unsigned getNumberOfBytes() const{
		unsigned numberOfBytes = 0;
		numberOfBytes += shardId.getNumberOfBytes();
		numberOfBytes += sizeof(unsigned);
		numberOfBytes += BLOCK_SIZE;
		return numberOfBytes;
	}
	void * deserialize(void * buffer){
		buffer = shardId.deserialize(buffer);
		buffer = srch2::util::deserializeFixedTypes(buffer, sequenceNumber);
		memcpy(data, buffer, BLOCK_SIZE);
		return buffer;
	}
};

struct MigrationDoneMsgBody{
	ClusterShardId shardId;
	void * serialize(void * buffer)const{
		buffer = shardId.serialize(buffer);
		return buffer;
	}
	unsigned getNumberOfBytes() const{
		unsigned numberOfBytes = 0;
		numberOfBytes += shardId.getNumberOfBytes();
		return numberOfBytes;
	}
	void * deserialize(void * buffer){
		buffer = shardId.deserialize(buffer);
		return buffer;
	}
};

struct MigrationDoneAckMsgBody{
	ClusterShardId shardId;
	char flag;  // 0 = incomplete or 1 = complete
	unsigned missingPacketCount;
	unsigned * getArr(){
		return (unsigned *)((char *)this + sizeof(MigrationDoneAckMsgBody));
	}
	void * serialize(void * buffer)const{
		ASSERT(false);//TODO
		return buffer;
	}
	unsigned getNumberOfBytes() const{
		unsigned numberOfBytes = 0;
		ASSERT(false);//TODO
		return numberOfBytes;
	}
	void * deserialize(void * buffer){
		ASSERT(false);//TODO
		return buffer;
	}
};


bool IndexSizeComparator(std::pair<string, long> l,  std::pair<string, long> r) {
	if (l.second > r.second)
		return true;   // put larger index first
	else
		return false;
}

int checkSocketIsReadyForRead(int socket, int waitingTime = 1) {
	/*
	 *  Prepare data structure for select system call.
	 *  http://man7.org/linux/man-pages/man2/select.2.html
	 */
	fd_set selectSet;
	timeval waitTimeout;
	waitTimeout.tv_sec = waitingTime;
	waitTimeout.tv_usec = 0;
	FD_ZERO(&selectSet);
	FD_SET(socket, &selectSet);


	/*
	 *   Wait until timeout = 1sec or until socket is ready for read/write. (whichever occurs first)
	 *   see select man page : http://linux.die.net/man/2/select
	 */
	int result = 0;
	// pass select set to read argument
	result = select(socket + 1, &selectSet, NULL, NULL, &waitTimeout);
	if (result == -1) {
		perror("error while waiting for a socket to become available for read/write!");
		return -1;
	}
	return FD_ISSET(socket, &selectSet) ? 1 : 0;
}

int readDataFromSocketWithRetry(int fd, char *buffer, int byteToRead, int retryCount) {

    int waitingTime = 1;
	while(1) {
		int readByte = recv(fd, buffer, byteToRead, MSG_DONTWAIT);
		if(readByte == 0) {
			// the connection is closed by peer. return status -1 (error)
			return -1;
		}

		if(readByte == -1) {
			if(errno == EAGAIN || errno == EWOULDBLOCK) {
				// socket is not ready for read. try again.
				if (retryCount) {
					int rc = checkSocketIsReadyForRead(fd, waitingTime);
					if(rc == -1){
	                    Logger::debug("read timeout");
	                    return -1;
					}else if(rc == 0){
					    waitingTime += 2;
					}else {
					    waitingTime = 1;
					}
					--retryCount;
					continue;
				} else {
					Logger::debug("read timeout");
					return -1;
				}
			} else {
				perror("Error while reading data from socket : ");
				//some socket error. return status -1 (error)
				return -1;
			}
		}

		if(readByte < byteToRead) {
			buffer += readByte;
			byteToRead -= readByte;
			continue;
		}

		ASSERT(byteToRead == readByte);
		return 0;
	}
	return 0;
}


struct MigrationThreadArguments {
	MigrationManager *mm;
	ClusterShardId shardId;
	unsigned remotedNode;
};

void *receiverThreadEntryPoint(void *arg) {

	MigrationThreadArguments * tArgs = (MigrationThreadArguments *)arg;
	MigrationService *clientService = new MigrationService(tArgs->mm);
	clientService->receiveShard(tArgs->shardId, tArgs->remotedNode);
	delete tArgs;
	return NULL;
}

void *senderThreadEntryPoint(void *arg) {

	MigrationThreadArguments * tArgs = (MigrationThreadArguments *)arg;
	MigrationService *clientService = new MigrationService(tArgs->mm);
	clientService->sendShard(tArgs->shardId, tArgs->remotedNode);
	delete tArgs;
	return NULL;
}

MMCallBackForTM::MMCallBackForTM(MigrationManager *mm) {
	this->migrationMgr = mm;
}

bool MMCallBackForTM::resolveMessage(Message * incomingMessage, NodeId remoteNode) {

	switch (incomingMessage->getType()) {
	case MigrationInitMessage:
	{
		MigrationInitMsgBody initMsgBodyObj;
		initMsgBodyObj.deserialize(incomingMessage->getMessageBody());
		MigrationInitMsgBody * initMsgBody = &initMsgBodyObj;

		migrationMgr->sessionLock.lock();
		if (!migrationMgr->hasActiveSession(initMsgBody->migratingShardId, remoteNode)) {

			string sessionkey = migrationMgr->initMigrationSession(initMsgBody->migratingShardId,
					initMsgBody->destinationShardId,
					initMsgBody->srcOperationId, initMsgBody->dstOperationId,
					initMsgBody->srcNodeId, initMsgBody->dstNodeId,
					remoteNode,
					initMsgBody->shardComponentCount);

			migrationMgr->migrationSession[sessionkey].status = MM_STATE_INIT_RCVD;

			pthread_t receiverThread;
			MigrationThreadArguments * arg = new MigrationThreadArguments();
			arg->mm = migrationMgr;
			arg->shardId = initMsgBody->migratingShardId;
			arg->remotedNode = remoteNode;
			pthread_create(&receiverThread, NULL, receiverThreadEntryPoint, arg);
			pthread_detach(receiverThread);
		}
		migrationMgr->sessionLock.unlock();
		break;
	}
	case MigrationComponentBeginMessage:
	{
		ShardComponentInfoMsgBody compInfoMsgBody;
		compInfoMsgBody.deserialize(incomingMessage->getMessageBody());
		string sessionkey;
		migrationMgr->sessionLock.lock();
		if (migrationMgr->hasActiveSession(compInfoMsgBody.shardId, remoteNode, sessionkey)) {
			migrationMgr->migrationSession[sessionkey].shardCompSize = compInfoMsgBody.componentSize;
			migrationMgr->migrationSession[sessionkey].shardCompName = compInfoMsgBody.componentName;
			migrationMgr->migrationSession[sessionkey].status = MM_STATE_INFO_RCVD;
		}
		migrationMgr->sessionLock.unlock();
		break;
	}
	case MigrationInitAckMessage:
	{
		MigrationInitAckMsgBody initAckMessageBody ;
		initAckMessageBody.deserialize(incomingMessage->getMessageBody());
		string sessionkey;
		migrationMgr->sessionLock.lock();
		if (migrationMgr->hasActiveSession(initAckMessageBody.shardId, remoteNode, sessionkey)) {
			migrationMgr->migrationSession[sessionkey].listeningPort = initAckMessageBody.portnumber;
			migrationMgr->migrationSession[sessionkey].remoteAddr = initAckMessageBody.ipAddress;
			migrationMgr->migrationSession[sessionkey].status = MM_STATE_INIT_ACK_RCVD;
		}
		migrationMgr->sessionLock.unlock();
		break;
	}
	case MigrationComponentBeginAckMessage:
	{
		//if (migrationMgr->migrationSession[compInfoMsgBody->shardId].status == MM_STATUS_ACK_RECEIVED)
		string sessionkey;
		ShardComponentInfoAckMsgBody compInfoMsgBody;
		compInfoMsgBody.deserialize(incomingMessage->getMessageBody());
		migrationMgr->sessionLock.lock();
		if (migrationMgr->hasActiveSession(compInfoMsgBody.shardId, remoteNode, sessionkey)) {
			migrationMgr->migrationSession[sessionkey].status = MM_STATE_INFO_ACK_RCVD;
		}
		migrationMgr->sessionLock.unlock();
		break;
	}
	case MigrationComponentEndAckMessage:
	{
		string sessionkey;
		MigrationDoneMsgBody completeMsgBody;
		completeMsgBody.deserialize(incomingMessage->getMessageBody());
		migrationMgr->sessionLock.lock();
		if (migrationMgr->hasActiveSession(completeMsgBody.shardId, remoteNode, sessionkey)) {
			migrationMgr->migrationSession[sessionkey].status = MM_STATE_COMPONENT_TRANSFERRED_ACK_RCVD;
		}
		migrationMgr->sessionLock.unlock();
		break;
	}
	case MigrationCompleteAckMessage:
	{
		string sessionkey;
		MigrationDoneMsgBody completeMsgBody ;
		completeMsgBody.deserialize(incomingMessage->getMessageBody());
		migrationMgr->sessionLock.lock();
		if (migrationMgr->hasActiveSession(completeMsgBody.shardId, remoteNode, sessionkey)) {
			migrationMgr->migrationSession[sessionkey].status = MM_STATE_SHARD_TRANSFERRED_ACK_RCVD;
		}
		migrationMgr->sessionLock.unlock();
		break;
	}
	default :
		ASSERT(false);
		break;

	}
	return true;
}

MigrationManager::MigrationManager(TransportManager *transport,
		ConfigManager *config) {
	this->shardManager = ShardManager::getShardManager();
	this->transport = transport;
	this->sendSocket = openSendChannel();
	this->configManager = config;
	transportCallback = new MMCallBackForTM(this);
	this->transport->registerCallbackHandlerForMM(transportCallback);
}

MigrationManager::~MigrationManager() {
	delete transportCallback;
}

void MigrationManager::sendMessage(unsigned destinationNodeId, Message *message) {
	message->setMigrationMask();
	transport->sendMessage(destinationNodeId, message);

}

void MigrationService::receiveShard(ClusterShardId shardId, unsigned remoteNode) {
	int receiveSocket;
	short receivePort;
	string sessionKey = migrationMgr->getSessionKey(shardId, remoteNode);
	migrationMgr->sessionLock.lock();
	MigrationSessionInfo& currentSessionInfo = migrationMgr->migrationSession[sessionKey];
	migrationMgr->sessionLock.unlock();

	currentSessionInfo.beginTimeStamp = time(NULL);


//	currentSessionInfo.print();

	migrationMgr->openTCPReceiveChannel(receiveSocket, receivePort);

	if (receiveSocket == -1) {
		//close socket
		Logger::debug("Migration: Network error while migrating shard %s", shardId.toString().c_str());
		migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
		return;
	}

	currentSessionInfo.listeningPort = receivePort;

	int listenStatus = migrationMgr->listenTCPConnection(receiveSocket, receivePort);
	if (listenStatus == -1) {
		//close socket
		Logger::debug("Migration: Network error while trying to listen. For shard %s", shardId.toString().c_str());
		close(receiveSocket);
		migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
		return;
	}

    Logger::sharding(Logger::Detail, "MM | Receiving shard %s from %d, sending first ack.", shardId.toString().c_str(), remoteNode);
	migrationMgr->sendInitMessageAck(currentSessionInfo);

	Logger::debug("waiting for sender to connect...");
    Logger::sharding(Logger::Detail, "MM | Receiving shard %s from %d, waiting for sender to connect ...", shardId.toString().c_str(), remoteNode);
	int commSocket = migrationMgr->acceptTCPConnection(receiveSocket, receivePort);
	if (commSocket == -1) {
		//close socket
		Logger::debug("Migration: Network error while trying to accept. For migrating shard %s", shardId.toString().c_str());
		close(receiveSocket);
		migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
		return;
	}

	unsigned componentCount = currentSessionInfo.shardCompCount;
    Logger::sharding(Logger::Detail, "MM | Receiving shard %s from %d, component count : %d",
            shardId.toString().c_str(), remoteNode, componentCount);
	// Create a path to store this Shard on storage device.
	ConfigManager *configManager = migrationMgr->configManager;
	string directoryPath = "";
	{
		boost::shared_ptr<const ClusterResourceMetadata_Readview> readview;
		ShardManager::getReadview(readview);
		directoryPath = configManager->createShardDir(readview->getClusterName(),
				readview->getCore(currentSessionInfo.destShardId.coreId)->getName(),
				&currentSessionInfo.destShardId);
	}
	Srch2Server *migratedShard = new Srch2Server(migrationMgr->getIndexConfig(shardId), directoryPath, "");

	unsigned mmapBufferSize = 0;
	void * mmapBuffer = NULL;
	unsigned maxMappedSize = 0;
	bool emptyshard = false;

	for (unsigned i =0; i < componentCount; ++i) {

	    Logger::sharding(Logger::Detail, "MM | Receiving shard %s from %d, waiting for component begin message (i : %d)",
	            shardId.toString().c_str(), remoteNode, i);
		migrationMgr->busyWaitWithTimeOut(currentSessionInfo, MM_STATE_INFO_RCVD);
		if(currentSessionInfo.status !=  MM_STATE_INFO_RCVD) {
	        Logger::sharding(Logger::Detail, "MM | Receiving shard %s from %d, TIMEOUT!!! (i : %d)",
	                shardId.toString().c_str(), remoteNode, i);
			close(commSocket);
			close(receiveSocket);
			delete migratedShard;
			migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
			return;
		}

		string filePath = directoryPath + "/" + currentSessionInfo.shardCompName;
		unsigned componentSize = currentSessionInfo.shardCompSize;

        Logger::sharding(Logger::Detail, "MM | Receiving shard %s from %d, component name : %s, component size : %d, filepath : %s (i : %d)",
                shardId.toString().c_str(), remoteNode,
                currentSessionInfo.shardCompName.c_str(), currentSessionInfo.shardCompSize, filePath.c_str(), i);

//		currentSessionInfo.print();

		int writeFd = -1;
		if (componentSize > 0) {
			writeFd = open(filePath.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0000644);
			if (writeFd == -1) {
				//close socket
				perror("");   // TODO: replace perror in SM/TM/MM with strerror_r
				Logger::debug("Migration:I/O error for shard %s", shardId.toString().c_str());
				close(commSocket);
				close(receiveSocket);
				delete migratedShard;
				migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
				return;
			}
		}

//		migrationMgr->sendInfoAckMessage(currentSessionInfo);

		unsigned sequenceNumber = 0;

		if (componentSize == 0) {
			std::istringstream inputStream(ios::binary);
			migratedShard->bootStrapShardComponentFromByteStream(inputStream, currentSessionInfo.shardCompName);
			emptyshard = true;
			Logger::debug("%u/%u Received", 0, componentSize);
			//Send ACK
			migrationMgr->sendComponentDoneMsg(currentSessionInfo);
			continue;
		}

//	    currentSessionInfo.print();
		int ftruncStatus = ftruncate(writeFd, componentSize);
		if (ftruncStatus == -1) {
			//close socket
			perror("truncate: ");   // TODO: replace perror in SM/TM/MM with strerror_r
			Logger::debug("Migration:I/O error for shard %s", shardId.toString().c_str());
			close(commSocket);
			close(receiveSocket);
			delete migratedShard;
			migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
			return;
		}

#ifdef __MACH__
		mmapBuffer = mmap(NULL, componentSize, PROT_WRITE|PROT_READ, MAP_PRIVATE, writeFd, 0);
#else
		mmapBuffer = mmap64(NULL, componentSize, PROT_WRITE|PROT_READ, MAP_SHARED, writeFd, 0);
#endif

		if (mmapBuffer == MAP_FAILED) {
			perror("mmap: ");   // TODO: replace perror in SM/TM/MM with strerror_r
			Logger::debug("Migration error for shard %s", shardId.toString().c_str());
			close(receiveSocket);
			close(commSocket);
			delete migratedShard;
			migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
			return;
		}

		long offset = 0;
		int retryCount = 10;
		while(offset < componentSize) {

			signed packetSize;

			if ((componentSize - sequenceNumber * BLOCK_SIZE) > BLOCK_SIZE) {
				packetSize = BLOCK_SIZE;
			}else {
				packetSize = componentSize - sequenceNumber * BLOCK_SIZE;
			}

			int status = readDataFromSocketWithRetry(commSocket, (char *)mmapBuffer + offset, packetSize, retryCount);
			if (status == -1) {
				//close socket
				perror("");
				Logger::debug("Migration: Network error while migrating shard %s", shardId.toString().c_str());
				munmap(mmapBuffer, componentSize);
				close(receiveSocket);
				close(commSocket);
				delete migratedShard;
				migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
				return;
			}

			offset += packetSize;
			++sequenceNumber;
		}
		Logger::debug("%u/%u Received", offset, componentSize);

//	    currentSessionInfo.print();

		std::istringstream inputStream(ios::binary);
		inputStream.rdbuf()->pubsetbuf((char *)mmapBuffer , componentSize);
		inputStream.seekg(0, ios::beg);

		migratedShard->bootStrapShardComponentFromByteStream(inputStream, currentSessionInfo.shardCompName);

		munmap(mmapBuffer, componentSize);
		close(writeFd);
		//Send ACK
		migrationMgr->sendComponentDoneMsg(currentSessionInfo);
	}

//	migrationMgr->sendComponentDoneMsg(currentSessionInfo);

	// create an empty shard
	if (emptyshard) {
		migratedShard->getIndexer()->commit();
	}
	migratedShard->postBootStrap();
	currentSessionInfo.shard.reset(migratedShard);

	Logger::debug("Saving shard to : %s", directoryPath.c_str());

	currentSessionInfo.endTimeStamp = time(NULL);
	Logger::console("Received shard %s in %d secs", shardId.toString().c_str(),
			currentSessionInfo.endTimeStamp - currentSessionInfo.beginTimeStamp);


//    currentSessionInfo.print();
	migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_SUCCESS);

	close(receiveSocket);
	close(commSocket);
}

void MigrationManager::populateStatus(ShardMigrationStatus& status, unsigned srcOperationId,
		unsigned dstOperationId, unsigned sourceNodeId, unsigned destinationNodeId,
		boost::shared_ptr<Srch2Server> shard,
		MIGRATION_STATUS migrationResult){

	status.srcOperationId = srcOperationId;
	status.dstOperationId = dstOperationId;
	status.sourceNodeId =  sourceNodeId;
	status.destinationNodeId = destinationNodeId;
	status.shard = shard;
	status.status = migrationResult;
}

void MigrationManager::notifySHMAndCleanup(string sessionKey, MIGRATION_STATUS migrationResult) {
	ShardMigrationStatus migrationStatus;
	sessionLock.lock();
	populateStatus(migrationStatus, migrationSession[sessionKey].srcOperationId,
			migrationSession[sessionKey].dstOperationId, migrationSession[sessionKey].srcNodeId,
			migrationSession[sessionKey].destNodeId, migrationSession[sessionKey].shard,
			migrationResult);
	migrationSession.erase(sessionKey);
	sessionLock.unlock();
	shardManager->resolveMMNotification(migrationStatus);
}

void MigrationManager::migrateShard(const ClusterShardId& currentShardId ,
		const boost::shared_ptr<Srch2Server>& shardPtr,
		const ClusterShardId& destShardId, const NodeOperationId & currentAddress,
		const NodeOperationId & requesterAddress) {

	Logger::console("Migrating shard %s to node %d", currentShardId.toString().c_str(), requesterAddress.nodeId);

	ShardMigrationStatus status;
	populateStatus(status, currentAddress.operationId, requesterAddress.operationId,
			currentAddress.nodeId, requesterAddress.nodeId, shardPtr,
			MM_STATUS_SUCCESS);
	shardManager->resolveMMNotification(status);
	return;
//	/*
//	 *  Initialize the migration session info
//	 */
//	sessionLock.lock();
//	if (hasActiveSession(currentShardId, requesterAddress.nodeId)) {
//		sessionLock.unlock();
//		ShardMigrationStatus status;
//		populateStatus(status, currentAddress.operationId, requesterAddress.operationId,
//				currentAddress.nodeId, requesterAddress.nodeId, shardPtr,
//				MM_STATUS_BUSY);
//		shardManager->resolveMMNotification(status);
//		return;
//	}
//	string sessionKey = this->initMigrationSession(currentShardId, destShardId,
//			currentAddress.operationId, requesterAddress.operationId,
//			currentAddress.nodeId, requesterAddress.nodeId, requesterAddress.nodeId, 0);
//	migrationSession[sessionKey].status = MM_STATE_INIT_RCVD;
//	migrationSession[sessionKey].shard = shardPtr;
//	sessionLock.unlock();
//
//	pthread_t senderThread;
//	MigrationThreadArguments * arg = new MigrationThreadArguments();
//	arg->mm = this;
//	arg->shardId = currentShardId;
//	arg->remotedNode = requesterAddress.nodeId;
//	pthread_create(&senderThread, NULL, senderThreadEntryPoint, arg);
//	pthread_detach(senderThread);

}
void MigrationService::sendShard(ClusterShardId shardId, unsigned destinationNodeId) {


	string sessionKey = migrationMgr->getSessionKey(shardId, destinationNodeId);
	migrationMgr->sessionLock.lock();
	// Pointers and references to elements are never invalidated. Hence it is safe to keep
	// the reference and use it going forward.
	MigrationSessionInfo& currentSessionInfo = migrationMgr->migrationSession[sessionKey];
	migrationMgr->sessionLock.unlock();

	boost::shared_ptr<Srch2Server>& shard = currentSessionInfo.shard;
	currentSessionInfo.beginTimeStamp = time(NULL);

	/*
	 *  Serialize indexes and calculate size of serialized indexes.
	 */

	Logger::debug("Save shard to disk...");
	shard->getIndexer()->save();

	vector<std::pair<string, long> > indexFilesWithSize;
	int status = shard->getSerializedShardSize(indexFilesWithSize);

	/*
	 *  Initiate handshaking with MM on the destination node by sending init message and waiting
	 *  for the acknowledgment.
	 */
	currentSessionInfo.shardCompCount = indexFilesWithSize.size();
	migrationMgr->doInitialHandShake(currentSessionInfo);

	// Note: we check the status of getSerializedShardSize function call now because it allows the
	//       the MM to start session with a remote node. Remote node's MM
	//       will time out automatically in case of an error happens here.
	if (status == -1) {
		// cannot determine indexes size
		Logger::debug("Unable to access shard on disk");
		migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
		return;
	}

	if (currentSessionInfo.status != MM_STATE_INIT_ACK_RCVD ) {
		Logger::debug("Unable to migrate shard ..destination node did not respond to the handshaking.");
		migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
		return;
	}

	/*
	 *  Make a TCP connection to remote node
	 */

	int sendSocket = migrationMgr->openTCPSendChannel(currentSessionInfo.remoteAddr,
			currentSessionInfo.listeningPort);
	if (sendSocket == -1) {
		Logger::debug("Unable to open TCP connection to send the shard.");
		migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
		return;
	}

	for (unsigned i = 0; i < indexFilesWithSize.size(); ++i) {
		const string& componentName = indexFilesWithSize[i].first;
		currentSessionInfo.shardCompName = componentName;
		unsigned componentSize = indexFilesWithSize[i].second;
		currentSessionInfo.shardCompSize = componentSize;

		Logger::sharding(Logger::Detail, "MM | Migrating shard component %s (size : %d) for migrating shard %s to %d",
		        currentSessionInfo.shardCompName.c_str(), currentSessionInfo.shardCompSize, shardId.toString().c_str(), destinationNodeId);
		migrationMgr->sendComponentInfoAndWaitForAck(currentSessionInfo);

//		migrationMgr->busyWaitWithTimeOut(currentSessionInfo, MM_STATE_INFO_ACK_RCVD);
//		if (currentSessionInfo.status != MM_STATE_INFO_ACK_RCVD) {
//			Logger::console("Unable to migrate shard ..destination node did not respond to begin");
//			migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
//			return;
//		}

		// read shard from disk and send through.
		string filename = shard->getIndexer()->getStoredIndexDirectory() + "/" + componentName;
		int inputFd = -1;
		if (componentSize > 0) {
			inputFd = open(filename.c_str(), O_RDONLY);  // TODO: Should lock as well?
			if (inputFd == -1) {
				Logger::debug("Shard could not be read from storage device");
				perror("");
				migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
				return;
			}
		}
        Logger::sharding(Logger::Detail, "MM | Sending data for component %s (size : %d) for migrating shard %s to %d",
                currentSessionInfo.shardCompName.c_str(), currentSessionInfo.shardCompSize, shardId.toString().c_str(), destinationNodeId);
		off_t offset = 0;

		if (componentSize > 0) {
		while(1) {
			off_t byteToSend = BLOCK_SIZE;
#ifdef __MACH__
			int status = sendfile(inputFd, sendSocket, offset, &byteToSend, NULL, 0);
			if (status == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
				perror("");
				Logger::debug("Network error while sending the shard");
				migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
				return;
			}

			if (status == 0 && byteToSend == 0)
				break;

			offset += byteToSend;
#else
			int status = sendfile(sendSocket, inputFd, &offset, byteToSend);
			if (status == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
				perror("");
				Logger::debug("Network error while sending the shard");
				migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
				return;
			}
			if (offset >= componentSize)
				break;
#endif

		}
		}
		Logger::debug("%u/%u sent", offset, componentSize);
		if(componentSize > 0){
			close(inputFd);
		}

		migrationMgr->busyWaitWithTimeOut(currentSessionInfo, MM_STATE_COMPONENT_TRANSFERRED_ACK_RCVD);
		if (currentSessionInfo.status != MM_STATE_COMPONENT_TRANSFERRED_ACK_RCVD) {
			Logger::debug("Unable to migrate shard ..destination node did not respond");
			migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_FAILURE);
			return;
		}
		Logger::debug("Data received by remote node...continuing");
	}

	close(sendSocket);

	currentSessionInfo.endTimeStamp = time(NULL);
	Logger::console("Send shard %s in %d secs", shardId.toString().c_str(),
			currentSessionInfo.endTimeStamp - currentSessionInfo.beginTimeStamp);
    Logger::sharding(Logger::Detail, "MM | Finished migrating shard %s to %d",
            shardId.toString().c_str(), destinationNodeId);

	migrationMgr->notifySHMAndCleanup(sessionKey, MM_STATUS_SUCCESS);

}

void MigrationManager::doInitialHandShake(MigrationSessionInfo& currentSessionInfo) {

	MigrationInitMsgBody *initMessageBody = new MigrationInitMsgBody;
	initMessageBody->migratingShardId = currentSessionInfo.currentShardId;
	initMessageBody->destinationShardId = currentSessionInfo.destShardId;

	initMessageBody->shardComponentCount = currentSessionInfo.shardCompCount;

	initMessageBody->dstOperationId = currentSessionInfo.dstOperationId;
	initMessageBody->srcOperationId = currentSessionInfo.srcOperationId;
	initMessageBody->srcNodeId = currentSessionInfo.srcNodeId;
	initMessageBody->dstNodeId = currentSessionInfo.destNodeId;

	Message *initMessage = MessageAllocator().allocateMessage(initMessageBody->getNumberOfBytes());
	initMessage->setType(MigrationInitMessage);
	initMessageBody->serialize(initMessage->getMessageBody());
	delete initMessageBody;
	unsigned destinationNodeId = currentSessionInfo.remoteNode;
	currentSessionInfo.status = MM_STATE_INIT_ACK_WAIT;

	//int tryCount = 5;
	int timewait = 10;
	time_t beg  = time(NULL);
	time_t end = beg;
	do {
		sendMessage(destinationNodeId, initMessage);
		if (difftime(end , beg) > timewait){
			break;
		}
		end = time(NULL);
		sleep(1);
	} while(currentSessionInfo.status != MM_STATE_INIT_ACK_RCVD);
	deAllocateMessage(initMessage);

	return;
}

void MigrationManager::sendInitMessageAck(MigrationSessionInfo& currentSessionInfo) {

	MigrationInitAckMsgBody body;
	body.shardId = currentSessionInfo.currentShardId;
	body.portnumber = currentSessionInfo.listeningPort;
	body.ipAddress = transport->getPublishedInterfaceNumericAddr();
	Message * initAckMesssage = MessageAllocator().allocateMessage(body.getNumberOfBytes());
	initAckMesssage->setType(MigrationInitAckMessage);
	body.serialize(initAckMesssage->getMessageBody());
	currentSessionInfo.status = MM_STATE_INFO_WAIT;
	sendMessage(currentSessionInfo.remoteNode, initAckMesssage);
	MessageAllocator().deallocateByMessagePointer(initAckMesssage);

}

void MigrationManager::sendInfoAckMessage(MigrationSessionInfo& currentSessionInfo) {
	ShardComponentInfoAckMsgBody body;
	body.shardId = currentSessionInfo.currentShardId;
	Message * infoAckMesssage = MessageAllocator().allocateMessage(body.getNumberOfBytes());
	infoAckMesssage->setType(MigrationComponentBeginAckMessage);
	body.serialize(infoAckMesssage->getMessageBody());
	currentSessionInfo.status = MM_STATE_COMPONENT_RECEIVING;
	Logger::debug("sending component begin ack to %d ", currentSessionInfo.remoteNode);
	sendMessage(currentSessionInfo.remoteNode, infoAckMesssage);
	MessageAllocator().deallocateByMessagePointer(infoAckMesssage);
}

void MigrationManager::sendComponentInfoAndWaitForAck(MigrationSessionInfo& currentSessionInfo) {

	unsigned destinationNodeId = currentSessionInfo.remoteNode;
	string componentName = currentSessionInfo.shardCompName;
	ShardComponentInfoMsgBody body;
	body.shardId = currentSessionInfo.currentShardId;
	body.componentSize = currentSessionInfo.shardCompSize;
	body.componentName = componentName;
	Message *compInfoMessage = allocateMessage(body.getNumberOfBytes());
	compInfoMessage->setType(MigrationComponentBeginMessage);
	body.serialize(compInfoMessage->getMessageBody());
	currentSessionInfo.status = MM_STATE_INFO_ACK_WAIT;
	sendMessage(destinationNodeId, compInfoMessage);
	deAllocateMessage(compInfoMessage);

	//busyWaitWithTimeOut(currentSessionInfo, MM_STATE_INFO_ACK_RCVD);
}

void MigrationManager::sendComponentDoneMsg(MigrationSessionInfo& currentSessionInfo) {

	const string& componentName  = currentSessionInfo.shardCompName;
	bool shardDone = componentName.size() > 0 ? false : true;
	MigrationDoneMsgBody body;
	body.shardId = currentSessionInfo.currentShardId;

	Message *compDoneMessage = allocateMessage(body.getNumberOfBytes());
	if (!shardDone) {
		compDoneMessage->setType(MigrationComponentEndAckMessage);
	} else {
		compDoneMessage->setType(MigrationCompleteAckMessage);
	}
	body.serialize(compDoneMessage->getMessageBody());

	if (!shardDone) {
		currentSessionInfo.status = MM_STATE_INFO_WAIT;  // waiting for next component
	} else {
		currentSessionInfo.status = MM_STATE_SHARD_RCVD;
	}
	sendMessage(currentSessionInfo.remoteNode, compDoneMessage);
	deAllocateMessage(compDoneMessage);

}

void MigrationManager::openTCPReceiveChannel(int& tcpSocket , short& receivePort) {
	/*
	 *  Prepare socket data structures.
	 */

	if((tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		tcpSocket = -1;
		perror("listening socket failed to init");
		return;
	}

	const int optVal = 1;
	setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, sizeof(optVal));


	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(sockaddr_in));
	addr.sin_family = AF_INET;
	inet_aton(transport->getPublisedInterfaceAddress().c_str(), &addr.sin_addr);
	receivePort = MM_MIGRATION_PORT_START;
	addr.sin_port = htons(receivePort);
	tryNextPort:
	if( bind(tcpSocket, (struct sockaddr *) &addr, sizeof(sockaddr_in)) < 0) {
		++receivePort;
		if (receivePort < MM_MIGRATION_PORT_START + 100) {
			addr.sin_port = htons(receivePort);
			goto tryNextPort;
		} else {
			perror("");
			Logger::debug("unable to bind to port for shard migration in range = [%d : %d]",
					MM_MIGRATION_PORT_START, MM_MIGRATION_PORT_START + 100);
			close(tcpSocket);
			tcpSocket = -1;
			return;
		}
	}
}
int MigrationManager::acceptTCPConnection(int tcpSocket , short receivePort) {
	int tcpReceiveSocket;
	fcntl(tcpSocket, F_SETFL, O_NONBLOCK);
	struct sockaddr_in senderAddress;
	unsigned sizeOfAddr = sizeof(senderAddress);
	unsigned timeWaitInSec = 10;  // TODO: define in header file.
	time_t beg = time(NULL);
	time_t end = beg;
	unsigned waitTime = 1;
	while(difftime(end , beg) < timeWaitInSec) {
		transport->checkSocketIsReadyForRead(tcpSocket,waitTime);
		tcpReceiveSocket = accept(tcpSocket, (struct sockaddr *) &senderAddress, &sizeOfAddr);
		if (tcpReceiveSocket == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				waitTime += 2;
			}else{
				perror("");
				return tcpReceiveSocket;
			}
		} else if (tcpReceiveSocket > 0) {
			break;
		} else {
			ASSERT(false);  // accept should not return 0
		}
		end = time(NULL);
	}
	return tcpReceiveSocket;
}

int MigrationManager::listenTCPConnection(int tcpSocket , short receivePort) {
	int tcpReceiveSocket;
	if(listen(tcpSocket, 20) == -1) {
		close(tcpSocket);
		perror("Listening on socket returned -1 : acceptTCPConnection");
		Logger::sharding(Logger::Error, "MM | Listening on socket %d returned -1", tcpSocket);
		return -1;
	}
	return 0;
}


void MigrationManager::openReceiveChannel(int& udpSocket , short& receivePort) {
	/*
	 *  Prepare socket data structures.
	 */
	if((udpSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("listening socket failed to init");
		exit(255);
	}
	/*
	 *   Make socket non blocking
	 */

	fcntl(udpSocket, F_SETFL, O_NONBLOCK);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(sockaddr_in));
	addr.sin_family = AF_INET;
	inet_aton(transport->getPublisedInterfaceAddress().c_str(), &addr.sin_addr);
	receivePort = MM_MIGRATION_PORT_START;
	addr.sin_port = htons(receivePort);
tryNextPort:
	if( bind(udpSocket, (struct sockaddr *) &addr, sizeof(sockaddr_in)) < 0){
		++receivePort;
		if (receivePort < MM_MIGRATION_PORT_START + 100) {
			addr.sin_port = htons(receivePort);
			goto tryNextPort;
		} else {
			perror("");
			Logger::debug("unable to bind to port for shard migration in range = [%d : %d]",
					MM_MIGRATION_PORT_START, MM_MIGRATION_PORT_START + 100);
			exit(-1);
		}
	}

}

int MigrationManager::openTCPSendChannel(unsigned remoteAddr, short remotePort) {
	/*
	 *  Prepare socket data structures.
	 */
	int tcpSocket;
	if((tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("sending socket failed to init");
		return -1;
	}

	struct sockaddr_in destinationAddress;
	memset(&destinationAddress, 0, sizeof(destinationAddress));
	destinationAddress.sin_family = AF_INET;
	destinationAddress.sin_addr.s_addr = remoteAddr;
	destinationAddress.sin_port = htons(remotePort);

	int status = connect(tcpSocket, (struct sockaddr *)&destinationAddress, sizeof(destinationAddress));

	if (status == -1) {
		perror("Unable to connect to remote node");
		return -1;
	}
	/*
	 *   Make socket non blocking
	 */
	//fcntl(tcpSocket, F_SETFL, O_NONBLOCK);//TODO

	return tcpSocket;

}

int MigrationManager::openSendChannel() {
	/*
	 *  Prepare socket data structures.
	 */
	int udpSocket;
	if((udpSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("sending socket failed to init");
		exit(255);
	}
	/*
	 *   Make socket non blocking
	 */
	//fcntl(udpSocket, F_SETFL, O_NONBLOCK);

	return udpSocket;

}

const CoreInfo_t * MigrationManager::getIndexConfig(ClusterShardId shardId) {
	boost::shared_ptr<const srch2::httpwrapper::ClusterResourceMetadata_Readview> clusterReadview;
	srch2::httpwrapper::ShardManager::getReadview(clusterReadview);
	return clusterReadview->getCore(shardId.coreId);
}

bool MigrationManager::hasActiveSession(const ClusterShardId& shardId, unsigned remoteNode) {
	string sessionKey;
	return hasActiveSession(shardId, remoteNode, sessionKey);
}
bool MigrationManager::hasActiveSession(const ClusterShardId& shardId, unsigned remoteNode, string& sessionKey) {
	sessionKey = getSessionKey(shardId, remoteNode);
	if (migrationSession.find(sessionKey) == migrationSession.end()) {
		return false;
	} else  {
		return true;
	}
}

string MigrationManager::initMigrationSession(const ClusterShardId& currentShardId,
		const ClusterShardId& destShardId, unsigned srcOperationId, unsigned dstOperationId,
		unsigned srcNodeId, unsigned dstNodeId,unsigned remoteNode, unsigned shardCompCount) {

	string key = getSessionKey(currentShardId, remoteNode);
	MigrationSessionInfo info;
	info.currentShardId = currentShardId;
	info.destShardId = destShardId;
	info.srcOperationId = srcOperationId;
	info.dstOperationId = dstOperationId;
	info.srcNodeId = srcNodeId;
	info.destNodeId = dstNodeId;
	info.beginTimeStamp = 0;
	info.endTimeStamp = 0;
	info.listeningPort = -1;
	info.status = MM_STATE_MIGRATION_BEGIN;
	info.remoteNode = remoteNode;
	info.shardCompCount = shardCompCount;
	//info.shard.reset();
	migrationSession[key] = info;
	return key;
}

void MigrationManager::busyWaitWithTimeOut(const MigrationSessionInfo& currentSessionInfo,
		MIGRATION_STATE expectedState, int waittime) {
	// wait for n seconds without sleep and check for status change.
	time_t prevTime = time(NULL);
	while(currentSessionInfo.status != expectedState) {
		time_t timeNow = time(NULL);
		if (timeNow - prevTime > waittime) {
			break;
		}
	}
}

} /* namespace httpwrapper */
} /* namespace srch2 */
